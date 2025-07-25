#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <dirent.h>
#include <errno.h>
#ifdef DIAG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imfs.h"

static Node g_nodes[MAX_NODES];

// Each Process (Cage) has it's own FD Table, all of which are initiated
// in memory when imfs_init() is called. Node are allocated using the use of
// g_next_node and g_free_list, as described below.
//
// This tracks "Holes" in the g_nodes table, caused by nodes that were deleted.
// When creating a new node, we check which index this free list points to and creates
// the node there. In case there are no free nodes in this list, we use the global
// g_next_node index.
static int g_next_node = 0;
static int g_free_list[MAX_NODES];
static int g_free_list_size = -1;

static FileDesc g_fdtable[MAX_PROCS][MAX_FDS];

// We use the same logic for fd allocations.
static int g_next_fd[MAX_PROCS];
static int g_fd_free_list[MAX_PROCS][MAX_FDS];
static int g_fd_free_list_size[MAX_PROCS];

static Node *g_root_node = NULL;

//
// String Utils
//

static size_t
str_len(const char *name)
{
	int i = 0;
	while (name[i] != '\0') {
		i++;
	}
	return i;
}

static char *
str_rchr(const char *s, const char c)
{
	char *last = 0;

	while (*s != '\0') {
		if (*s == (char)c) {
			last = (char *)s;
		}
		s++;
	}

	if (c == '\0') {
		return (char *)s;
	}

	return last;
}

static void
split_path(const char *path, int *count, char namecomp[MAX_DEPTH][MAX_NODE_NAME])
{
	*count = 0;

	int i = 0;
	if (path[i] == '/')
		i++;

	int current_len = 0;
	while (path[i] != '\0') {
		if (path[i] == '/') {
			namecomp[*count][current_len] = '\0';
			(*count)++;
			current_len = 0;
		} else {
			namecomp[*count][current_len++] = path[i];
		}

		i++;
	}
	namecomp[*count][current_len] = '\0';
	(*count)++;
}

static int
str_compare(const char *a, const char *b)
{
	int a_len = 0;
	while (a[a_len] != '\0')
		a_len++;
	int b_len = 0;
	while (b[b_len] != '\0')
		b_len++;

	if (a_len != b_len)
		return 0;
	int i = 0, j = 0;
	while (a[i] != '\0' && b[j] != '\0') {
		if (a[i] != b[j])
			return 0;
		i++;
		j++;
	}
	return 1;
}

static void
str_ncopy(char *dst, const char *src, int n)
{
	size_t i;
	for (i = 0; i < n && src[i] != '\0'; i++) {
		dst[i] = src[i];
	}
}

static void
mem_cpy(void *dst, const void *src, size_t n)
{
	size_t i;
	unsigned char *d = dst;
	const unsigned char *s = src;

	for (i = 0; i < n; i++) {
		d[i] = s[i];
	}
}

//
//  IMFS Utils
//

static Node *
imfs_create_node(const char *name, NodeType type, mode_t mode)
{
	if (g_free_list_size == -1 && g_next_node >= MAX_NODES) {
		errno = ENOMEM;
		return NULL;
	}

	int node_index;
	if (g_free_list_size == -1)
		node_index = g_next_node++;
	else
		node_index = g_free_list[g_free_list_size--];

	Node *node = &g_nodes[node_index];

	if (node->type != M_NON) {
		errno = ENOMEM;
		return NULL;
	}

	node->in_use = 0;
	node->type = type;
	node->size = 0;
	node->d_count = 0;
	node->d_children = NULL;
	node->r_data = NULL;
	node->parent = NULL;
	node->mode = node->type | (mode & 0777);

	str_ncopy(node->name, name, MAX_NODE_NAME - 1);
	node->name[MAX_NODE_NAME - 1] = '\0';
	return node;
}

static int
imfs_allocate_fd(int cage_id, Node *node)
{
	if (!node)
		return -1;

	int i;
	if (g_fd_free_list_size[cage_id] > -1) {
		i = g_fd_free_list[cage_id][g_fd_free_list_size[cage_id]--];
	} else {
		i = g_next_fd[cage_id]++;
	}

	if (i == MAX_FDS) {
		errno = EMFILE;
		return -1;
	}

	g_fdtable[cage_id][i] = (FileDesc) {
		.node = node,
		.offset = 0,
		.link = NULL,
	};

	node->in_use++;

	return i;
}

static int
imfs_dup_fd(int cage_id, int oldfd, int newfd)
{
	if (newfd == oldfd)
		return newfd;

	int i;
	if (newfd != -1) {
		i = newfd;
		goto allocate;
	}

	if (g_fd_free_list_size[cage_id] > -1) {
		i = g_fd_free_list[cage_id][g_fd_free_list_size[cage_id]--];
	} else {
		i = g_next_fd[cage_id]++;
	}

	if (i == MAX_FDS) {
		errno = EMFILE;
		return -1;
	}

allocate:

	if (g_fdtable[cage_id][i].node || g_fdtable[cage_id][i].link)
		imfs_close(cage_id, i);

	g_fdtable[cage_id][i] = (FileDesc) {
		.link = &g_fdtable[cage_id][oldfd],
		.node = NULL,
		.offset = 0,
	};

	return i;
}

static FileDesc *
get_filedesc(int cage_id, int fd)
{
	if (g_fdtable[cage_id][fd].link)
		return g_fdtable[cage_id][fd].link;

	return &g_fdtable[cage_id][fd];
}

static Node *
imfs_find_node_namecomp(int cage_id, int dirfd, const char namecomp[MAX_DEPTH][MAX_NODE_NAME], int count)
{
	FileDesc *fd = get_filedesc(cage_id, dirfd);
	if (count == 0)
		return g_root_node;

	Node *current;
	if (dirfd == AT_FDCWD)
		current = g_root_node;
	else
		current = fd->node;

	for (int i = 0; i < count && current; i++) {
		Node *found = NULL;
		for (size_t j = 0; j < current->d_count; j++) {
			if (str_compare(namecomp[i], current->d_children[j].name) == 1) {
				switch (current->d_children[j].node->type) {
				case M_LNK:
					found = current->d_children[j].node->l_link;
					break;
				case M_DIR:
				case M_REG:
					found = current->d_children[j].node;
					break;
				default:
					found = NULL;
				}
				break;
			}
		}

		if (!found) {
			return NULL;
		}

		current = found;
	}

	return current;
}

static Node *
imfs_find_node(int cage_id, int dirfd, const char *path)
{
	if (!path || !g_root_node)
		return NULL;

	if (path[0] == '/' && path[1] == '\0')
		return g_root_node;

	int count;
	char namecomps[MAX_DEPTH][MAX_NODE_NAME];

	split_path(path, &count, namecomps);

	return imfs_find_node_namecomp(cage_id, dirfd, namecomps, count);
}

static int
add_child(Node *parent, Node *node)
{
	if (!parent || !node || parent->type != M_DIR)
		return -1;

	size_t new_count = parent->d_count + 1;
	DirEnt *new_children = realloc(parent->d_children, new_count * sizeof(DirEnt));

	if (!new_children)
		return -1;

	new_children[parent->d_count].node = node;

	parent->d_children = new_children;

	str_ncopy(parent->d_children[parent->d_count].name, node->name, MAX_NODE_NAME - 1);
	parent->d_count = new_count;
	node->parent = parent;

	return 0;
}

static int
imfs_remove_file(Node *node)
{
	node->parent->d_count--;
	node->doomed = 1;

	if (!node->in_use) {
		g_free_list[++g_free_list_size] = node->index;
		node->type = M_NON;
	}

	return 0;
}

static int
imfs_remove_dir(Node *node)
{
	if (node == g_root_node || node->d_children > 0) {
		errno = EBUSY;
		return -1;
	}

	if (!node->in_use) {
		g_free_list[++g_free_list_size] = node->index;
		node->type = M_NON;
	}

	node->parent->d_count--;
	node->doomed = 1;
	return 0;
}

static int
imfs_remove_link(Node *node)
{
	if (!node->in_use) {
		g_free_list[++g_free_list_size] = node->index;
		node->type = M_NON;
	}

	node->doomed = 1;
	node->parent->d_count--;
	return 0;
}

static ssize_t
__imfs_read(int cage_id, int fd, void *buf, size_t count, int pread, off_t offset)
{
	FileDesc *c_fd = get_filedesc(cage_id, fd);

	if (fd < 0 || fd >= MAX_FDS || !c_fd->node || !buf || offset < 0) {
		errno = EBADF;
		return -1;
	}

	if (offset < 0) {
		errno = EINVAL;
		return -1;
	}

	Node *node = c_fd->node;

	if (node->type != M_REG) {
		errno = EISDIR;
		return -1;
	}

	if (c_fd->offset >= node->size) {
		return 0;
	}

	size_t available = node->size - c_fd->offset;
	size_t to_read = count < available ? count : available;

	off_t use_offset = pread ? offset : c_fd->offset;

	mem_cpy(buf, node->r_data + use_offset, to_read);
	if (!pread)
		c_fd->offset += to_read;

	return to_read;
}

static ssize_t
__imfs_readv(int cage_id, int fd, const struct iovec *iov, int len, off_t offset, int pread)
{
	int ret, fin = 0;
	for (int i = 0; i < len; i++) {
		ret = __imfs_read(cage_id, fd, iov[i].iov_base, iov[i].iov_len, 0, 0);
		if (ret == -1)
			return ret;
		else
			fin += ret;
	}

	return fin;
}

static ssize_t
__imfs_write(int cage_id, int fd, const void *buf, size_t count, int pread, off_t offset)
{
	if (fd < 0 || fd >= MAX_FDS) {
		errno = EBADF;
		return -1;
	}

	if (offset < 0) {
		errno = EINVAL;
		return -1;
	}

	FileDesc *fdesc = get_filedesc(cage_id, fd);

	Node *node = fdesc->node;
	if (node->type != M_REG) {
		errno = EISDIR;
		return -1;
	}

	size_t new_size = fdesc->offset + count;
	if (new_size > node->size) {
		char *new_data = realloc(node->r_data, new_size);

		node->r_data = new_data;
		node->size = new_size;
	}

	off_t use_offset = pread ? offset : fdesc->offset;

	mem_cpy(node->r_data + use_offset, buf, count);

	if (!pread)
		fdesc->offset += count;

	return count;
}

static ssize_t
__imfs_writev(int cage_id, int fd, const struct iovec *iov, int count, off_t offset, int pread)
{
	int ret, fin = 0;
	for (int i = 0; i < count; i++) {
		ret = __imfs_write(cage_id, fd, iov[i].iov_base, iov[i].iov_len, pread, count);
		if (ret == -1)
			return ret;
		else
			fin += ret;
	}
	return fin;
}

static int
__imfs_stat(int cage_id, Node *node, struct stat *statbuf)
{
	if (node == NULL)
		return -1;
	*statbuf = (struct stat) {
		.st_dev = GET_DEV,
		.st_ino = node->index,
		.st_mode = node->mode,
		.st_nlink = 1,
		.st_uid = GET_UID,
		.st_gid = GET_GID,
		.st_rdev = 0,
		.st_size = node->size,
		.st_blksize = 512,
		.st_blocks = node->size / 512,
	};

	return 0;
}

void
imfs_init(void)
{
	for (int cage_id = 0; cage_id < MAX_PROCS; cage_id++) {
		for (int i = 0; i < MAX_FDS; i++) {
			g_fdtable[cage_id][i] = (FileDesc) {
				.node = NULL,
				.offset = 0,
			};
		}
	}

	for (int i = 0; i < MAX_NODES; i++) {
		g_nodes[i] = (Node) {
			.type = M_NON,
			.index = i,
			.in_use = 0,
			.d_count = 0,
			.size = 0,
			.info = NULL,
			.mode = 0,
		};
	}

	for (int i = 0; i < MAX_PROCS; i++) {
		g_fd_free_list_size[i] = -1;
	}

	for (int i = 0; i < MAX_PROCS; i++) {
		g_next_fd[i] = 3;
	}

	g_root_node = imfs_create_node("/", M_DIR, 0755);
	g_root_node->parent = g_root_node;

	Node *dot = imfs_create_node(".", M_LNK, 0);
	if (!dot)
		exit(1);
	dot->l_link = g_root_node;

	Node *dotdot = imfs_create_node("..", M_LNK, 0);
	if (!dotdot)
		exit(1);

	if (add_child(g_root_node, dot) != 0)
		exit(1);
	if (add_child(g_root_node, dotdot) != 0)
		exit(1);
	dotdot->l_link = g_root_node;
}

//
// FS Entrypoints
//

int
imfs_openat(int cage_id, int dirfd, const char *path, int flags, mode_t mode)
{
	if (!path) {
		errno = EINVAL;
		return -1;
	}

	if (dirfd == -1) {
		errno = EBADF;
		return -1;
	}

	Node *node = imfs_find_node(cage_id, dirfd, path);

	// New File
	if (!node) {
		if (!(flags & O_CREAT)) {
			errno = ENOENT;
			return -1;
		}

		int count;
		char namecomp[MAX_DEPTH][MAX_NODE_NAME];

		split_path(path, &count, namecomp);

		char *filename = namecomp[count - 1];

		Node *parent_node;

		parent_node = imfs_find_node_namecomp(cage_id, dirfd, namecomp, count - 1);

		if (!parent_node || parent_node->type != M_DIR) {
			errno = ENOTDIR;
			return -1;
		}

		node = imfs_create_node(filename, M_REG, mode);
		if (!node) {
			return -1;
		}

		if (add_child(parent_node, node) != 0) {
			errno = ENOMEM;
			node->type = M_NON;
			return -1;
		}
	} else {
		// File Exists
		if (/*flags & O_EXCL ||*/ flags & O_CREAT) {
			errno = EEXIST;
			return -1;
		}

		if (node->type == M_DIR && !(flags & O_DIRECTORY)) {
			errno = EISDIR;
			return -1;
		}

		// Check for file access based on flags and mode.

		switch (O_ACCMODE & flags) {
		case O_RDONLY:
			if (!(node->mode & S_IROTH)) {
				errno = EACCES;
				return -1;
			}
			break;
		case O_RDWR:
			if (!(node->mode & S_IWOTH) || !(node->mode & S_IROTH)) {
				errno = EACCES;
				return -1;
			}
			break;
		case O_WRONLY:
			if (!(node->mode & S_IWOTH)) {
				errno = EACCES;
				return -1;
			}
			break;
		default:
			break;
		}
	}

	return imfs_allocate_fd(cage_id, node);
}

int
imfs_open(int cage_id, const char *path, int flags, mode_t mode)
{
	return imfs_openat(cage_id, AT_FDCWD, path, flags, mode);
}

int
imfs_creat(int cage_id, const char *path, mode_t mode)
{
	return imfs_open(cage_id, path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int
imfs_close(int cage_id, int fd)
{
	if (fd < 0 || fd >= MAX_FDS || !g_fdtable[cage_id][fd].node) {
		errno = EBADF;
		return -1;
	}

	FileDesc *fdesc = get_filedesc(cage_id, fd);
	fdesc->node->in_use--;

	if (fdesc->node->doomed) {
		fdesc->node->type = M_NON;
		g_free_list[++g_free_list_size] = fdesc->node->index;
	}

	g_fd_free_list[cage_id][++g_fd_free_list_size[cage_id]] = fd;

	*fdesc = (FileDesc) {
		.node = NULL,
		.offset = 0,
	};

	return 0;
}

ssize_t
imfs_write(int cage_id, int fd, const void *buf, size_t count)
{
	return __imfs_write(cage_id, fd, buf, count, 0, 0);
}

ssize_t
imfs_pwrite(int cage_id, int fd, const void *buf, size_t count, off_t offset)
{
	return __imfs_write(cage_id, fd, buf, count, 1, offset);
}

ssize_t
imfs_writev(int cage_id, int fd, const struct iovec *iov, int count)
{
	return __imfs_writev(cage_id, fd, iov, count, 0, 0);
}

ssize_t
imfs_pwritev(int cage_id, int fd, const struct iovec *iov, int count, off_t offset)
{
	return __imfs_writev(cage_id, fd, iov, count, offset, 1);
}

ssize_t
imfs_read(int cage_id, int fd, void *buf, size_t count)
{
	return __imfs_read(cage_id, fd, buf, count, 0, 0);
}

ssize_t
imfs_pread(int cage_id, int fd, void *buf, size_t count, off_t offset)
{
	return __imfs_read(cage_id, fd, buf, count, 1, offset);
}

ssize_t
imfs_readv(int cage_id, int fd, const struct iovec *iov, int count)
{
	return __imfs_readv(cage_id, fd, iov, count, 0, 0);
}

ssize_t
imfs_preadv(int cage_id, int fd, const struct iovec *iov, int count, off_t offset)
{
	return __imfs_readv(cage_id, fd, iov, count, offset, 1);
}

int
imfs_mkdirat(int cage_id, int fd, const char *path, mode_t mode)
{
	if (!path) {
		errno = EINVAL;
		return -1;
	}

	Node *parent;

	char namecomp[MAX_DEPTH][MAX_NODE_NAME];
	int count;

	split_path(path, &count, namecomp);
	char *filename = namecomp[count - 1];

	if (str_compare(filename, ".") || str_compare(filename, "..")) {
		errno = EINVAL;
		return -1;
	}

	parent = imfs_find_node_namecomp(cage_id, fd, namecomp, count - 1);
	if (!parent) {
		errno = EINVAL;
		return -1;
	}

	Node *node = imfs_create_node(filename, M_DIR, mode);
	if (!node) {
		return -1;
	}

	if (add_child(parent, node) != 0) {
		errno = ENOMEM;
		node->type = M_NON;
		return -1;
	}

	Node *dot = imfs_create_node(".", M_LNK, 0);
	if (!dot)
		return -1;
	dot->l_link = node;

	Node *dotdot = imfs_create_node("..", M_LNK, 0);
	if (!dotdot)
		return -1;

	if (add_child(node, dot) != 0)
		return -1;
	if (add_child(node, dotdot) != 0)
		return -1;

	dotdot->l_link = node->parent;

	LOG("Created Node: \n");
	LOG("Index: %d \n", node->index);
	LOG("Name: %s\n", node->name);
	LOG("Type: %d\n", node->type);

	return 0;
}

int
imfs_mkdir(int cage_id, const char *path, mode_t mode)
{
	LOG("Making dir. %s | %d \n", path, mode);
	return imfs_mkdirat(cage_id, AT_FDCWD, path, mode);
}

int
imfs_linkat(int cage_id, int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)
{
	Node *oldnode = imfs_find_node(cage_id, olddirfd, oldpath);

	if (!oldnode) {
		errno = EINVAL;
		return -1;
	}

	char namecomp[MAX_DEPTH][MAX_NODE_NAME];
	int count;

	Node *newnode = imfs_find_node(cage_id, newdirfd, newpath);
	if (newnode != NULL) {
		errno = EINVAL;
		return -1;
	}

	split_path(newpath, &count, namecomp);

	char *filename = namecomp[count - 1];

	Node *newnode_parent = imfs_find_node_namecomp(cage_id, newdirfd, namecomp, count - 1);
	newnode = imfs_create_node(filename, M_LNK, 0);

	newnode->l_link = oldnode;

	if (add_child(newnode_parent, newnode) != 0) {
		errno = ENOMEM;
		newnode->type = M_NON;
		return -1;
	}

	return 0;
}

int
imfs_link(int cage_id, const char *oldpath, const char *newpath)
{
	return imfs_linkat(cage_id, AT_FDCWD, oldpath, AT_FDCWD, newpath, 0);
}

int
imfs_symlink(int cage_id, const char *oldpath, const char *newpath)
{
	return imfs_linkat(cage_id, AT_FDCWD, oldpath, AT_FDCWD, newpath, 0);
}

int
imfs_rename(int cage_id, const char *oldpath, const char *newpath)
{
	// TODO
	return 0;
}

int
imfs_chown(int cage_id, const char *pathname, uid_t owner, gid_t group)
{
	// TODO
	return 0;
}

int
imfs_remove(int cage_id, const char *pathname)
{
	Node *node = imfs_find_node(cage_id, AT_FDCWD, pathname);

	if (!node) {
		errno = ENOENT;
		return -1;
	}

	// if (node->in_use) {
	// 	errno = EBUSY;
	// 	return -1;
	// }

	switch (node->type) {
	case M_DIR:
		return imfs_remove_dir(node);
	case M_LNK:
		return imfs_remove_link(node);
	case M_REG:
		return imfs_remove_file(node);
	default:
		return 0;
	}
}

int
imfs_rmdir(int cage_id, const char *pathname)
{
	return imfs_remove(cage_id, pathname);
}

int
imfs_unlink(int cage_id, const char *pathname)
{
	return imfs_remove(cage_id, pathname);
}

off_t
imfs_lseek(int cage_id, int fd, off_t offset, int whence)
{
	FileDesc *fdesc = get_filedesc(cage_id, fd);

	if (!fdesc->node) {
		errno = EBADF;
		return -1;
	}

	off_t ret = fdesc->offset;

	// SEEK_HOLE and SEEK_DATA need to be reworked. Unclear as to what it is they do
	switch (whence) {
	case SEEK_SET:
		ret = offset;
		break;
	case SEEK_CUR:
		ret += offset;
		break;
	case SEEK_END:
		ret = fdesc->node->size;
		break;
	case SEEK_HOLE:
		while (*(char *)(fdesc->node + ret)) {
			ret++;
		}
		break;
	case SEEK_DATA:
		while (!*(char *)(fdesc->node + ret)) {
			ret++;
		}
		break;
	default:
		errno = EINVAL;
		return ret - 1;
	}

	fdesc->offset = ret;

	return ret;
}

int
imfs_dup(int cage_id, int fd)
{
	return imfs_dup_fd(cage_id, fd, -1);
}

int
imfs_dup2(int cage_id, int oldfd, int newfd)
{
	return imfs_dup_fd(cage_id, oldfd, newfd);
}

int
imfs_lstat(int cage_id, const char *pathname, struct stat *statbuf)
{
	Node *node = imfs_find_node(cage_id, AT_FDCWD, pathname);
	return __imfs_stat(cage_id, node, statbuf);
}

int
imfs_stat(int cage_id, const char *pathname, struct stat *statbuf)
{
	LOG("cage=%d pathname=%s\n", cage_id, pathname);
	Node *node = imfs_find_node(cage_id, AT_FDCWD, pathname);
	if (node->type == M_LNK)
		return __imfs_stat(cage_id, node->l_link, statbuf);
	return __imfs_stat(cage_id, node, statbuf);
}

int
imfs_fstat(int cage_id, int fd, struct stat *statbuf)
{
	Node *node = get_filedesc(cage_id, fd)->node;
	if (node->type == M_LNK)
		return __imfs_stat(cage_id, node->l_link, statbuf);
	return __imfs_stat(cage_id, node, statbuf);
}

I_DIR *
imfs_opendir(int cage_id, const char *name)
{
	I_DIR *dirstream = NULL;
	int fd = imfs_open(cage_id, name, O_DIRECTORY, 0);
	Node *node = get_filedesc(cage_id, fd)->node;

	*dirstream = (I_DIR) {
		.fd = fd,
		.node = node,
		.size = 0,
		.offset = 0,
		.filepos = 0,
	};

	return dirstream;
}

struct dirent *
imfs_readdir(int cage_id, I_DIR *dirstream)
{
	struct dirent *ret = malloc(sizeof(struct dirent));

	Node *dirnode = dirstream->node;

	if (dirstream->offset >= dirnode->d_count) {
		return NULL;
	}

	// Next entry

	struct DirEnt nextentry = dirnode->d_children[dirstream->offset++];

	int ino = nextentry.node->index;
	int _type = nextentry.node->type;
	size_t namelen = str_len(nextentry.name);

	*ret = (struct dirent) {
		.d_ino = ino,	// 8
		.d_reclen = 32, // 24
		// .d_namlen = namelen,  // 32 + X
		.d_type = _type, // 36 + X
	};

	str_ncopy(ret->d_name, nextentry.name, namelen);
	ret->d_name[namelen + 1] = '\0';

	return ret;
}

//
// Main func for local testing.
//

#ifndef LIB
int
main()
{
	imfs_init();
	LOG("[imfs] Init...\n");
	int cage_id = 0;

	int fd = imfs_open(cage_id, "/firstfile.txt", O_CREAT | O_WRONLY, 0666);
	imfs_close(cage_id, fd);

	I_DIR *dirstream = imfs_opendir(cage_id, "/");

	struct dirent *entry;

	while ((entry = imfs_readdir(cage_id, dirstream)) != NULL) {
		LOG("%s\n", entry->d_name);
	}

	return 0;
}
#endif

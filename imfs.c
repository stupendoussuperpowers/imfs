#include <sys/fcntl.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "imfs.h"

// Each Process (Cage) has it's own FD Table, all of which are initiated
// in memory when imfs_init() is called. Node are allocated using the use of
// g_next_node and g_free_list, as described below.
static FileDesc g_fdtable[MAX_PROCS][MAX_FDS];
static Node g_nodes[MAX_NODES];
static int g_next_node = 0;

// This tracks "Holes" in the g_nodes table, caused by nodes that were deleted.
// When creating a new node, we check which index this free list points to and creates
// the node there. In case there are no free nodes in this list, we use the global
// g_next_node index.
static int g_free_list[MAX_NODES];
static int g_free_list_size = -1;

static Node *g_root_node = NULL;

//
// String Utils
//

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

	char current[256];
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

	for (; i < n; i++) {
		dst[i] = '\0';
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
imfs_create_node(const char *name, NodeType type)
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

	str_ncopy(node->name, name, MAX_NODE_NAME - 1);
	node->name[MAX_NODE_NAME - 1] = '\0';
	return node;
}

static int
imfs_allocate_fd(int cage_id, Node *node)
{
	if (!node)
		return -1;

	int i = 3;
	while (g_fdtable[cage_id][i].node != NULL || g_fdtable[cage_id][i].link != NULL)
		i++;

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
imfs_allocate_fd_dup(int cage_id, int oldfd, int newfd)
{
	if (newfd == oldfd)
		return newfd;

	int i = 3;
	if (newfd != -1) {
		i = newfd;
		goto allocate;
	}

	while (g_fdtable[cage_id][i].node != NULL || g_fdtable[cage_id][i].link != NULL)
		i++;

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
	g_free_list[++g_free_list_size] = node->index;
	node->type = M_NON;

	node->parent->d_count--;
	return 0;
}

static int
imfs_remove_dir(Node *node)
{
	if (node == g_root_node || node->d_children > 0) {
		errno = EBUSY;
		return -1;
	}

	g_free_list[++g_free_list_size] = node->index;
	node->type = M_NON;

	node->parent->d_count--;
	return 0;
}

static int
imfs_remove_link(Node *node)
{
	g_free_list[++g_free_list_size] = node->index;
	node->type = M_NON;

	node->parent->d_count--;
	return 0;
}

void
imfs_init()
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
		};
	}

	g_root_node = imfs_create_node("/", M_DIR);
	g_root_node->parent = g_root_node;

	Node *dot = imfs_create_node(".", M_LNK);
	if (!dot)
		exit(1);
	dot->l_link = g_root_node;

	Node *dotdot = imfs_create_node("..", M_LNK);
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

		node = imfs_create_node(filename, M_REG);
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
		if (flags & O_EXCL && flags & O_CREAT) {
			errno = EEXIST;
			return -1;
		}

		if (node->type == M_DIR && !(flags & O_DIRECTORY)) {
			errno = EISDIR;
			return -1;
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

	*fdesc = (FileDesc) {
		.node = NULL,
		.offset = 0,
	};

	return 0;
}

ssize_t
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

	mem_cpy(node->r_data + fdesc->offset, buf, count);

	if (!pread)
		fdesc->offset += count;

	return count;
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

	Node *node = imfs_create_node(filename, M_DIR);
	if (!node) {
		return -1;
	}

	if (add_child(parent, node) != 0) {
		errno = ENOMEM;
		node->type = M_NON;
		return -1;
	}

	Node *dot = imfs_create_node(".", M_LNK);
	if (!dot)
		return -1;
	dot->l_link = node;

	Node *dotdot = imfs_create_node("..", M_LNK);
	if (!dotdot)
		return -1;

	if (add_child(node, dot) != 0)
		return -1;
	if (add_child(node, dotdot) != 0)
		return -1;

	dotdot->l_link = node->parent;

	return 0;
}

int
imfs_mkdir(int cage_id, const char *path, mode_t mode)
{
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
	newnode = imfs_create_node(filename, M_LNK);

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
imfs_remove(int cage_id, const char *pathname)
{
	Node *node = imfs_find_node(cage_id, AT_FDCWD, pathname);

	if (!node) {
		errno = ENOENT;
		return -1;
	}

	if (node->in_use) {
		errno = EBUSY;
		return -1;
	}

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
	return imfs_allocate_fd_dup(cage_id, fd, -1);
}

int
imfs_dup2(int cage_id, int oldfd, int newfd)
{
	return imfs_allocate_fd_dup(cage_id, oldfd, newfd);
}

//
// Main func for local testing.
//

#ifndef LIB
int
main()
{
	imfs_init();
	printf("IMFS init...\n");
	int cage_id = 0;

	printf("[write]\n");
	int fd = imfs_open(cage_id, "/firstfile.txt", O_CREAT | O_WRONLY, 0);
	printf("fd: %d\n", fd);
	char *buf = "hello world";
	imfs_write(cage_id, fd, buf, 11);
	imfs_close(cage_id, fd);

	fd = imfs_open(cage_id, "/firstfile.txt", O_RDONLY, 0);
	int newfd = imfs_dup(cage_id, fd);

	char *readbuf;
	char *dupbuf;
	for (int i = 0; i < 11; i++) {
		imfs_read(cage_id, fd, readbuf, 1);
		imfs_read(cage_id, newfd, dupbuf, 1);
	}

	return 0;
}
#endif

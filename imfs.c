#include <sys/fcntl.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "imfs.h"

static struct FileDesc g_fdtable[MAX_FDS];
static struct Node g_nodes[MAX_NODES];
static struct Node *g_root_node = NULL;

static int g_next_fd = 0;
static int g_next_node = 0;

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

void
split_path(const char *path, int *count, char namecomp[MAX_DEPTH][MAX_NODE_NAME])
{
	*count = 0;

	int i = 1;

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

	printf("count: %d, path: %s\n", *count, path);
}

int
str_compare(char *a, char *b)
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

void
str_ncopy(char *dst, char *src, int n)
{
	size_t i;
	for (i = 0; i < n && src[i] != '\0'; i++) {
		dst[i] = src[i];
	}

	for (; i < n; i++) {
		dst[i] = '\0';
	}
}

void
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

Node *
imfs_create_node(const char *name, NodeType type)
{
	if (g_next_node >= MAX_NODES) {
		errno = ENOMEM;
		return NULL;
	}

	Node *node = &g_nodes[g_next_node++];
	node->in_use = 1;
	node->type = type;
	node->size = 0;
	node->count = 0;
	node->children = NULL;
	node->data = NULL;
	node->parent = NULL;

	str_ncopy(node->name, name, MAX_NODE_NAME - 1);
	node->name[MAX_NODE_NAME - 1] = '\0';
	return node;
}

int
imfs_allocate_fd(Node *node)
{
	if (!node)
		return -1;

	int i = 3;
	while (g_fdtable[i].node != NULL)
		i++;

	if (i == MAX_FDS) {
		errno = EMFILE;
		return -1;
	}

	g_fdtable[i] = (struct FileDesc) {
		.node = node,
		.offset = 0,
	};

	return i;
}

Node *
imfs_find_node_namecomp(int dirfd, const char namecomp[MAX_DEPTH][MAX_NODE_NAME], int count)
{
	if (count == 0)
		return g_root_node;

	Node *current;
	if (dirfd == AT_FDCWD)
		current = g_root_node;
	else
		current = g_fdtable[dirfd].node;

	for (int i = 0; i < count && current; i++) {
		Node *found = NULL;
		printf("Finding: %s\n", namecomp[i]);

		for (size_t j = 0; j < current->count; j++) {
			if (str_compare(namecomp[i], current->children[j].name) == 1) {
				switch (current->children[j].node->type) {
				case M_LNK:
					found = current->children[j].node->link;
					break;
				case M_DIR:
				case M_REG:
					found = current->children[j].node;
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

Node *
imfs_find_node(int dirfd, const char *path)
{
	if (!path || !g_root_node)
		return NULL;

	if (path[0] == '/' && path[1] == '\0')
		return g_root_node;

	int count;
	char namecomps[MAX_DEPTH][MAX_NODE_NAME];

	split_path(path, &count, namecomps);

	return imfs_find_node_namecomp(dirfd, namecomps, count);
}

int
add_child(Node *parent, Node *node)
{
	if (!parent || !node || parent->type != M_DIR)
		return -1;

	size_t new_count = parent->count + 1;
	DirEnt *new_children = realloc(parent->children, new_count * sizeof(DirEnt));

	if (!new_children)
		return -1;

	new_children[parent->count].node = node;

	parent->children = new_children;

	str_ncopy(parent->children[parent->count].name, node->name, MAX_NODE_NAME - 1);
	parent->count = new_count;
	node->parent = parent;

	return 0;
}

static void
imfs_init()
{
	for (int i = 0; i < MAX_FDS; i++) {
		g_fdtable[i] = (struct FileDesc) {
			.node = NULL,
			.offset = 0,
		};
	}

	for (int i = 0; i < MAX_NODES; i++) {
		g_nodes[i] = (struct Node) {
			.type = M_NON,
			.in_use = 0,
			.children = NULL,
			.count = 0,
			.data = NULL,
			.size = 0,
		};
	}

	g_root_node = imfs_create_node("/", M_DIR);
	g_root_node->parent = g_root_node;

	Node *dot = imfs_create_node(".", M_LNK);
	if (!dot)
		exit(1);
	dot->link = g_root_node;

	Node *dotdot = imfs_create_node("..", M_LNK);
	if (!dotdot)
		exit(1);

	if (add_child(g_root_node, dot) != 0)
		exit(1);
	if (add_child(g_root_node, dotdot) != 0)
		exit(1);
	dotdot->link = g_root_node;
}

//
// FS Entrypoints
//

int
imfs_openat(int dirfd, const char *path, int flags, mode_t mode)
{
	if (!path) {
		errno = EINVAL;
		return -1;
	}

	if (dirfd == -1) {
		errno = EBADF;
		return -1;
	}

	Node *node = imfs_find_node(dirfd, path);

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

		parent_node = imfs_find_node_namecomp(dirfd, namecomp, count - 1);

		if (!parent_node || parent_node->type != M_DIR) {
			errno = ENOTDIR;
			return -1;
		}

		node = imfs_create_node(filename, M_REG);
		if (!node) {
			return -1;
		}

		if (add_child(parent_node, node) != 0) {
			node->in_use = 0;
			errno = ENOMEM;
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

	return imfs_allocate_fd(node);
}

int
imfs_open(const char *path, int flags, mode_t mode)
{
	return imfs_openat(AT_FDCWD, path, flags, mode);
}

int
imfs_close(int fd)
{
	if (fd < 0 || fd >= MAX_FDS || !g_fdtable[fd].node) {
		errno = EBADF;
		return -1;
	}

	g_fdtable[fd] = (struct FileDesc) {
		.node = NULL,
		.offset = 0,
	};

	return 0;
}

ssize_t
imfs_write(int fd, const void *buf, size_t count)
{
	if (fd < 0 || fd >= MAX_FDS) {
		errno = EBADF;
		return -1;
	}

	Node *node = g_fdtable[fd].node;
	if (node->type != M_REG) {
		errno = EISDIR;
		return -1;
	}

	size_t new_size = g_fdtable[fd].offset + count;
	if (new_size > node->size) {
		char *new_data = realloc(node->data, new_size);

		node->data = new_data;
		node->size = new_size;
	}

	mem_cpy(node->data + g_fdtable[fd].offset, buf, count);
	g_fdtable[fd].offset += count;

	return count;
}

ssize_t
imfs_read(int fd, void *buf, size_t count)
{
	struct FileDesc c_fd = g_fdtable[fd];

	if (fd < 0 || fd >= MAX_FDS || !c_fd.node || !buf) {
		errno = EBADF;
		return -1;
	}

	Node *node = c_fd.node;

	if (node->type != M_REG) {
		errno = EISDIR;
		return -1;
	}

	if (c_fd.offset >= node->size) {
		return 0;
	}

	size_t available = node->size - c_fd.offset;
	size_t to_read = count < available ? count : available;

	mem_cpy(buf, node->data + c_fd.offset, to_read);
	c_fd.offset += to_read;

	g_fdtable[fd] = c_fd;

	return to_read;
}

int
imfs_mkdirat(int fd, const char *path, mode_t mode)
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

	parent = imfs_find_node_namecomp(fd, namecomp, count - 1);

	Node *node = imfs_create_node(filename, M_DIR);
	if (!node) {
		return -1;
	}

	if (add_child(parent, node) != 0) {
		node->in_use = 0;
		errno = ENOMEM;
		return -1;
	}

	Node *dot = imfs_create_node(".", M_LNK);
	if (!dot)
		return -1;
	dot->link = node;

	Node *dotdot = imfs_create_node("..", M_LNK);
	if (!dotdot)
		return -1;

	if (add_child(node, dot) != 0)
		return -1;
	if (add_child(node, dotdot) != 0)
		return -1;

	dotdot->link = node->parent;

	return 0;
}

int
imfs_mkdir(const char *path, mode_t mode)
{
	return imfs_mkdirat(AT_FDCWD, path, mode);
}

int
linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)
{
	Node *oldnode = imfs_find_node(olddirfd, oldpath);

	if (!oldnode) {
		errno = EINVAL;
		return -1;
	}

	char namecomp[MAX_DEPTH][MAX_NODE_NAME];
	int count;

	split_path(newpath, &count, namecomp);

	char *filename = namecomp[count - 1];

	Node *newnode_parent = imfs_find_node_namecomp(newdirfd, namecomp, count - 1);
	Node *newnode = imfs_create_node(filename, M_LNK);

	newnode->link = oldnode;

	if (add_child(newnode_parent, newnode) != 0) {
		newnode->in_use = 0;
		errno = ENOMEM;
		return -1;
	}

	return 0;
}

int
link(const char *oldpath, const char *newpath)
{
	return linkat(AT_FDCWD, oldpath, AT_FDCWD, newpath, 0);
}

//
// Main func for local testing.
//

int
main()
{
	imfs_init();
	printf("IMFS init...\n");

	printf("[write]\n");
	int fd = imfs_open("/firstfile.txt", O_CREAT | O_WRONLY, 0);
	printf("fd: %d\n", fd);

	ssize_t bytes = imfs_write(fd, "hello", 6);
	printf("bytes: %d\n", bytes);

	printf("close: %d\n", imfs_close(fd));

	printf("[read]\n");
	fd = imfs_open("/firstfile.txt", O_RDONLY, 0);
	printf("fd: %d\n", fd);
	char buf[6];
	bytes = imfs_read(fd, buf, 6);
	printf("read: %s\n", buf);
	printf("close: %d\n", imfs_close(fd));

	printf("[openat]\n");
	fd = open("target", O_RDONLY | O_DIRECTORY, 0);
	printf("fd: %d\n", fd);

	int fdat = openat(fd, "random/randfile.txt", O_RDONLY, 0);
	printf("fdat: %d\n", fdat);

	imfs_close(fdat);
	imfs_close(fd);

	printf("[mkdir]\n");
	int mkd = imfs_mkdir("/folder", 0);
	mkd = imfs_mkdir("/folder/folder_2", 0);

	fd = imfs_open("/folder/folder_2/file_3.txt", O_CREAT | O_WRONLY, 0);
	printf("fd: %d\n", fd);
	imfs_close(fd);

	fd = imfs_open("/folder/./folder_2/file_3.txt", O_RDONLY, 0);
	printf("fd: %d\n", fd);
	imfs_close(fd);

	fd = imfs_open("/folder/folder_2/../folder_2", O_RDONLY | O_DIRECTORY, 0);
	printf("fd: %d\n", fd);
	imfs_close(fd);

	return 0;
}

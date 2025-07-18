
#include <sys/types.h>

#include <fcntl.h>
#include <stddef.h>

#define MAX_NODE_NAME 64
#define MAX_NODE_SIZE 4096
#define MAX_FDS		  1024
#define MAX_NODES	  1024
#define MAX_DEPTH	  10

typedef enum {
	M_REG,
	M_DIR,
	M_LNK,
	M_NON,
} NodeType;

typedef struct Node {
	NodeType type;
	size_t size;

	// M_REG
	char *data;

	// M_LNK
	struct Node *link;

	// M_DIR
	struct DirEnt *children;
	size_t count;

	char name[MAX_NODE_NAME];
	struct Node *parent;
	int in_use;
} Node;

typedef struct DirEnt {
	char name[MAX_NODE_NAME];
	struct Node *node;
} DirEnt;

struct FileDesc {
	int stat;
	Node *node;
	int offset;
};

int imfs_open(const char *path, int flags, mode_t mode);
int imfs_openat(int dirfd, const char *path, int flags, mode_t mode);
ssize_t imfs_read(int fd, void *buf, size_t count);
ssize_t imfs_write(int fd, const void *buf, size_t count);
int imfs_close(int fd);
int imfs_mkdir(const char *path, mode_t mode);
int imfs_mkdirat(int fd, const char *path, mode_t mode);
int imfs_rmdir(const char *path);
int link(const char *oldpath, const char *newpath);
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int imfs_unlink(const char *path);
off_t imfs_lseek(int fd, off_t offset, int whence);

Node *imfs_find_node(int dirfd, const char *path);
Node *imfs_create_node(const char *name, NodeType type);
int imfs_allocate_fd(Node *node);
void imfs_free_fd(int fd);

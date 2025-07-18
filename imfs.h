
#include <sys/types.h>

#include <fcntl.h>
#include <stddef.h>

#define MAX_NODE_NAME 64
#define MAX_NODE_SIZE 4096
#define MAX_FDS		  1024
#define MAX_NODES	  1024
#define MAX_DEPTH	  10
#define MAX_PROCS	128

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

int imfs_open(int cage_id, const char *path, int flags, mode_t mode);
int imfs_openat(int cage_id, int dirfd, const char *path, int flags, mode_t mode);
ssize_t imfs_read(int cage_id, int fd, void *buf, size_t count);
ssize_t imfs_write(int cage_id, int fd, const void *buf, size_t count);
int imfs_close(int cage_id, int fd);
int imfs_mkdir(int cage_id, const char *path, mode_t mode);
int imfs_mkdirat(int cage_id, int fd, const char *path, mode_t mode);
int imfs_rmdir(int cage_id, const char *path);
int imfs_link(int cage_id, const char *oldpath, const char *newpath);
int imfs_linkat(int cage_id, int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int imfs_unlink(int cage_id, const char *path);
off_t imfs_lseek(int cage_id, int fd, off_t offset, int whence);

Node *imfs_find_node(int cage_id, int dirfd, const char *path);
Node *imfs_create_node(const char *name, NodeType type);
int imfs_allocate_fd(int cage_id, Node *node);
void imfs_free_fd(int cage_id, int fd);

void imfs_init();

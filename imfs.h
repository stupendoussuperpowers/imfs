
#include <sys/types.h>

#include <fcntl.h>
#include <stddef.h>

#define MAX_NODE_NAME 64
#define MAX_NODE_SIZE 4096
#define MAX_FDS		  1024
#define MAX_NODES	  1024

typedef enum {
	M_REG,
	M_DIR,
} NodeType;

typedef struct Node {
	NodeType type;
	size_t size;

	char *data;

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

// POSIX-style function declarations
int imfs_open(const char *path, int flags, mode_t mode);
int imfs_openat(int dirfd, const char *path, int flags, mode_t mode);
ssize_t imfs_read(int fd, void *buf, size_t count);
ssize_t imfs_write(int fd, const void *buf, size_t count);
int imfs_close(int fd);
int imfs_mkdir(const char *path, mode_t mode);
int imfs_rmdir(const char *path);
int imfs_unlink(const char *path);
off_t imfs_lseek(int fd, off_t offset, int whence);

// Load and dump functions
int imfs_load_from_disk(const char *base_path);
int imfs_dump_to_disk(const char *base_path);
int imfs_load_file(const char *disk_path, const char *imfs_path);
int imfs_dump_file(const char *imfs_path, const char *disk_path);

// Internal helper functions
Node *imfs_find_node(int dirfd, const char *path);
Node *imfs_create_node(const char *name, NodeType type);
int imfs_allocate_fd(Node *node);
void imfs_free_fd(int fd);

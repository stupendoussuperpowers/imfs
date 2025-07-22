
#include <sys/types.h>

#include <fcntl.h>
#include <stddef.h>

#define MAX_NODE_NAME	64
#define MAX_NODE_SIZE	4096
#define MAX_FDS		1024
#define MAX_NODES	1024
#define MAX_DEPTH	10
#define MAX_PROCS	128

typedef enum {
	M_REG,
	M_DIR,
	M_LNK,
	// Indicated free node
	M_NON,
} NodeType;

#define d_children	info.dir.children
#define d_count		info.dir.count
#define l_link		info.lnk.link
#define r_data		info.reg.data

typedef struct Node {
	NodeType type;
	int index;			/* Index in the global g_nodes */
	size_t size;			/* Size for offset related calls. */

	char name[MAX_NODE_NAME];	/* File name */ 
	struct Node *parent;		/* Parent node */
	int in_use;			/* Number of FD's attached to this node */
	
	union {
		// M_REG
		struct {
			char *data;			/* File contents stored as a char array */
		} reg;

		// M_LNK
		struct {
			struct Node *link;		/* Point to linked node. */
		} lnk;

		// M_DIR
		struct {
			struct DirEnt *children;	/* Directory contents. */
			size_t count;			/* len(children) including . and .. */
		} dir;
	} info;

} Node;

typedef struct DirEnt {
	char name[MAX_NODE_NAME];
	struct Node *node;
} DirEnt;

typedef struct FileDesc {
	int stat;
	struct FileDesc *link;
	Node *node;
	int offset;	/* How many bytes have been read. */
} FileDesc;

int imfs_open(int cage_id, const char *path, int flags, mode_t mode);
int imfs_openat(int cage_id, int dirfd, const char *path, int flags, mode_t mode);
ssize_t imfs_read(int cage_id, int fd, void *buf, size_t count);
ssize_t imfs_write(int cage_id, int fd, const void *buf, size_t count);
int imfs_close(int cage_id, int fd);
int imfs_mkdir(int cage_id, const char *path, mode_t mode);
int imfs_mkdirat(int cage_id, int fd, const char *path, mode_t mode);
int imfs_rmdir(int cage_id, const char *path);
int imfs_remove(int cage_id, const char *path);
int imfs_link(int cage_id, const char *oldpath, const char *newpath);
int imfs_linkat(int cage_id, int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int imfs_unlink(int cage_id, const char *path);
off_t imfs_lseek(int cage_id, int fd, off_t offset, int whence);
int imfs_dup(int cage_id, int oldfd);
int imfs_dup2(int cage_id, int oldfd, int newfd);

void imfs_init();

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imfs.h"

static struct FileDesc g_fdtable[MAX_FDS];
static struct Node g_nodes[MAX_NODES];
static struct Node *g_root_node = NULL;

static int g_next_fd = 0;
static int g_next_node = 0;

static void
imfs_init()
{
	for (int i = 0; i < MAX_FDS; i++) {
		g_fdtable[i] = (struct FileDesc) {
			.node = NULL,
			.offset = 0,
			.is_prestat = 0,
		};
	}

	for (int i = 0; i < MAX_NODES; i++) {
		g_nodes[i] = (struct Node) {
			.in_use = 0,
			.children = NULL,
			.count = 0,
			.data = NULL,
			.size = 0,
		};
	}

	g_root_node = imfs_create_node("/", M_DIR);
	g_root_node->parent = g_root_node;
}

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

	strncpy(node->name, name, MAX_NODE_NAME - 1);
	node->name[MAX_NODE_NAME - 1] = '\0';
	return node;
}

int
imfs_allocate_fd(Node *node)
{
	if(!node) return -1;
	
	int i = 0;
	while(g_fdtable[i].node != NULL) 
		i++;

	if(i == MAX_FDS) {
		errno = EMFILE;
		return -1;
	}

	g_fdtable[i] = (struct FileDesc) {
		.node = node, 
		.offset = 0, 
		.is_prestat = 0,
	};

	return i;
}

void
split_path(const char *path, int *count, char namecomp[10][256]) {
	*count = 0;

	int i=1;
	
	char current[256];
	int current_len = 0;
	
	while(path[i] != '\0') {
		if(path[i] == '/') {
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

int
str_compare(char *a, char *b)
{
	int i=0,j=0;
	while(a[i] != '\0' && b[j] != '\0') {
		if(a[i] != b[j])
			return 0;
		i++;
		j++;
	}
	return 1;
}

Node*
imfs_find_node_namecomp(const char namecomp[10][256], int count)
{
	Node *current = g_root_node;

	for(int i=0; i < count && current; i++) {
		Node *found = NULL;

		for(size_t j=0; j < current->count; j++) {
			if(str_compare(current->children[j].name, namecomp[i]) == 1) {
				found = current->children[j].node;
				break;
			}
		}

		if(!found) {
			return NULL;
		}

		current = found;
	}
	
	return current;
}

Node*
imfs_find_node(const char *path)
{
	if(!path || !g_root_node) 
		return NULL;

	if(path[0] == '/' && path[1] == '\0')
		return g_root_node;

	int count;
	char namecomps[10][256];

	split_path(path, &count, namecomps);

	return imfs_find_node_namecomp(namecomps, count);	
}

static char*
str_rchr(const char *s, const char c)
{
    char *last = 0;

    while (*s != '\0') {
        if (*s == (char)c) {
            last = (char *)s;  // cast away const to match strrchr signature
        }
        s++;
    }

    // Check the null terminator if c == '\0'
    if (c == '\0') {
        return (char *)s;
    }

    return last;
}

int
imfs_open(const char *path, int flags, mode_t mode)
{
	if (!path) {
		errno = EINVAL;
		return -1;
	}

	Node *node = imfs_find_node(path);

	// New File
	if (!node) {
		if (!(flags & O_CREAT)) {
			errno = ENOENT;
			return -1;
		}
		// ... 
		// ...
			
		int count; 
		char namecomp[10][256];

		split_path(path, &count, namecomp);

		char *filename = namecomp[count];

		Node* parent_node = imfs_find_node_namecomp(namecomp, count);

		if(!parent_node || parent_node->type != M_DIR) {
			errno = ENOTDIR;
			return -1;
		}
		
		node = imfs_create_node(filename, M_REG);
		if(!node) {
			return -1;
		}

		if(add_child(parent_node, node) != 0) {
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

		if (node->type == M_DIR) {
			errno = EISDIR;
			return -1;
		}
	}

	return imfs_allocate_fd(node);
}

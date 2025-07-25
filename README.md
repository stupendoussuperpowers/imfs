## In Memory File System

[![Make Tests](https://github.com/stupendoussuperpowers/imfs/actions/workflows/tests.yml/badge.svg?branch=main)](https://github.com/stupendoussuperpowers/imfs/actions/workflows/tests.yml)

Emulate a POSIX-compliant FS interface with nodes stored in memory.

[Reference](https://github.com/Lind-Project/lind-wasm/issues/304#issuecomment-3097608727)

### Building

To build as a library:

`make lib`

To build with the main function:

`make imfs`

To build and run tests:

```
make test           #Run all tests
make test-<prefix>  #Run all tests in the group starting with the prefix
```

Make imfs with debug symbols:

`make debug`

### Implemented FS Functions

```
int imfs_open(int cage_id, const char *path, int flags, mode_t mode);
int imfs_openat(int cage_id, int dirfd, const char *path, int flags, mode_t mode);
int imfs_creat(int cage_id, const char *path, mode_t mode);
int imfs_close(int cage_id, int fd);

int imfs_mkdir(int cage_id, const char *path, mode_t mode);
int imfs_mkdirat(int cage_id, int fd, const char *path, mode_t mode);
int imfs_rmdir(int cage_id, const char *path);

int imfs_remove(int cage_id, const char *path);

int imfs_link(int cage_id, const char *oldpath, const char *newpath);
int imfs_linkat(int cage_id, int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int imfs_unlink(int cage_id, const char *path);

ssize_t imfs_read(int cage_id, int fd, void *buf, size_t count);
ssize_t imfs_pread(int cage_id, int fd, void *buf, size_t count, off_t offset);

ssize_t imfs_write(int cage_id, int fd, const void *buf, size_t count);
ssize_t imfs_pwrite(int cage_id, int fd, const void *buf, size_t count, off_t offset);

off_t imfs_lseek(int cage_id, int fd, off_t offset, int whence);

int imfs_dup(int cage_id, int oldfd);
int imfs_dup2(int cage_id, int oldfd, int newfd);

int imfs_lstat(int cage_id, const char *pathname, struct stat *statbuf);
int imfs_stat(int cage_id, const char *pathname, struct stat *statbuf);
int imfs_fstat(int cage_id, int fd, struct stat *statbuf);

ssize_t imfs_readv(int cage_id, int fd, const struct iovec *iov, int count);
ssize_t imfs_preadv(int cage_id, int fd, const struct iovec *iov, int count, off_t offset);
ssize_t imfs_writev(int cage_id, int fd, const struct iovec *iov, int count);
ssize_t imfs_pwritev(int cage_id, int fd, const struct iovec *iov, int count, off_t offset);

```

#include <sys/fcntl.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "imfs.h"

#define CAGE_ID 1

#define OPEN(path, flags, mode, imfd, afd)              \
	do {                                                \
		(imfd) = imfs_open(CAGE_ID, path, flags, mode); \
		(afd) = open(path, flags, mode);                \
	} while (0)

#define CLOSE(imfd, afd, ret1, ret2)        \
	do {                                    \
		(ret1) = imfs_close(CAGE_ID, imfd); \
		(ret2) = close(afd);                \
	} while (0)

#define WRITE(imfd, afd, buf, len, ret1, ret2)        \
	do {                                              \
		(ret1) = imfs_write(CAGE_ID, imfd, buf, len); \
		(ret2) = write(afd, buf, len);                \
	} while (0)

#define PWRITE(imfd, afd, buf, len, off, ret1, ret2)        \
	do {                                                    \
		(ret1) = imfs_pwrite(CAGE_ID, imfd, buf, len, off); \
		(ret2) = pwrite(afd, buf, len, off);                \
	} while (0)

#define READ(imfd, afd, buf1, buf2, len, ret1, ret2)  \
	do {                                              \
		(ret1) = imfs_read(CAGE_ID, imfd, buf1, len); \
		(ret2) = read(afd, buf2, len);                \
	} while (0)

#define PREAD(imfd, afd, buf1, buf2, len, off, ret1, ret2)  \
	do {                                                    \
		(ret1) = imfs_pread(CAGE_ID, imfd, buf1, len, off); \
		(ret2) = pread(afd, buf2, len, off);                \
	} while (0)

#define MKDIR(path, mode, ret1, ret2)             \
	do {                                          \
		(ret1) = imfs_mkdir(CAGE_ID, path, mode); \
		(ret2) = mkdir(path, mode);               \
	} while (0)

#define UNLINK(path, ret1, ret2)             \
	do {                                     \
		(ret1) = imfs_unlink(CAGE_ID, path); \
		(ret2) = unlink(path);               \
	} while (0)

#define LSEEK(imfd, afd, offset, whence, ret1, ret2)        \
	do {                                                    \
		(ret1) = imfs_lseek(CAGE_ID, imfd, offset, whence); \
		(ret2) = lseek(afd, offset, whence);                \
	} while (0)

#define DUP(imfd, afd, ret1, ret2)        \
	do {                                  \
		(ret1) = imfs_dup(CAGE_ID, imfd); \
		(ret2) = dup(afd);                \
	} while (0)

#define DUP2(imfd, afd, newfd1, newfd2, ret1, ret2) \
	do {                                            \
		(ret1) = imfs_dup2(CAGE_ID, imfd, newfd1);  \
		(ret2) = dup2(afd, newfd2);                 \
	} while (0)

struct Test {
	char *name;
	char *group;
	int status;
	int err;
};

static struct Test testlist[128];
static int testcount = 0, passed = 0, failed = 0;
static char *group_filter;

typedef int (*TestFunc)(void);

void
load_folder(char *path)
{
	imfs_mkdir(CAGE_ID, path, 0);

	DIR *dir = opendir(path);
	struct dirent *entry;

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		char fullpath[4096];
		snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

		struct stat st;
		if (stat(fullpath, &st) == -1) {
			perror(fullpath);
			continue;
		}

		if (S_ISREG(st.st_mode)) {
			FILE *fp = fopen(fullpath, "rb");
			if (!fp)
				continue;

			int imfs_fd = imfs_open(CAGE_ID, fullpath, O_CREAT | O_WRONLY, 0);

			char buffer[1024];
			size_t nread;
			while ((nread = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
				imfs_write(CAGE_ID, imfs_fd, buffer, nread);
			}

			fclose(fp);
			imfs_close(CAGE_ID, imfs_fd);
		} else {
			load_folder(fullpath);
		}
	}
}

int
test_imfs_open_exist()
{
	int fd = imfs_open(CAGE_ID, "/test_folder/hello.txt", O_RDONLY, 0);

	if (fd < 0) {
		perror("");
		return 1;
	}
	if (imfs_close(CAGE_ID, fd) != 0) {
		perror("");
		return 1;
	}

	return 0;
}

int
test_imfs_open_create()
{
	int fd = imfs_open(CAGE_ID, "/test_folder/file_imfs.txt", O_CREAT | O_WRONLY, 0);
	if (fd < 0) {
		return 1;
	}

	if (imfs_close(CAGE_ID, fd) != 0) {
		return 1;
	}

	return 0;
}

int
test_imfs_openclose_invalid()
{
	int fd = imfs_open(CAGE_ID, "/test_folder/file_inexist.txt", O_RDONLY, 0);
	if (fd > 0) {
		return 1;
	}

	return 0;
}

int
test_dup()
{
	int fd, afd;
	OPEN("test_folder/hello.txt", O_RDONLY, 0644, fd, afd);

	if (fd < 0 || afd < 0)
		return 1;

	int d_fd, ad_fd;
	DUP(fd, afd, d_fd, ad_fd);

	if (d_fd < 0 || ad_fd < 0)
		return 1;

	char first[5], a_first[5];
	char second[5], a_second[5];

	int ret1, ret2;
	READ(fd, afd, first, a_first, 5, ret1, ret2);

	if (ret1 != ret2)
		return 1;

	if (strncmp(a_first, first, 5))
		return 1;

	READ(d_fd, ad_fd, second, a_second, 5, ret1, ret2);

	if (ret1 != ret2)
		return 1;

	if (strncmp(a_second, second, 5))
		return 1;

	return 0;
}

int
test_read()
{
	int fd, afd;
	OPEN("test_folder/hello.txt", O_RDONLY, 0644, fd, afd);

	if (fd < 0)
		return 1;

	char buf1[7] = "", buf2[7] = "";

	int ret1, ret2;

	READ(fd, afd, buf1, buf2, 7, ret1, ret2);

	if (ret1 != ret2)
		return 1;

	if (strncmp(buf1, buf2, 7))
		return 1;

	CLOSE(fd, afd, ret1, ret2);
	if (ret1 != ret2)
		return 1;

	return 0;
}

int
test_pread()
{
	int fd, afd;
	OPEN("test_folder/hello.txt", O_RDONLY, 0644, fd, afd);

	if (fd < 0)
		return 1;

	char buf1[7] = "", buf2[7] = "";

	int ret1, ret2;
	PREAD(fd, afd, buf1, buf2, 2, 0, ret1, ret2);
	if (ret1 != ret2)
		return 1;

	if (strncmp(buf1, buf2, 2))
		return 1;

	PREAD(fd, afd, buf1, buf2, 2, 0, ret1, ret2);
	if (ret1 != ret2)
		return 1;
	if (strncmp(buf1, buf2, 2))
		return 1;

	return 0;
}

int
test_write()
{
	char *buf = "hello world";
	int fd, afd;

	OPEN("test_folder/test_write.txt", O_CREAT | O_WRONLY, 0644, fd, afd);

	if (afd == -1 || fd == -1)
		return 1;

	int ret1, ret2;

	WRITE(fd, afd, buf, 10, ret1, ret2);

	if (ret1 != ret2)
		return 1;

	CLOSE(fd, afd, ret1, ret2);
	return 0;
}

int
test_pwrite()
{
	char *hello = "hello";
	char *world = "world";

	int fd, afd;
	OPEN("test_folder/test_write.txt", O_RDWR, 0644, fd, afd);

	if (afd == -1 || fd == -1)
		return 1;

	int ret1, ret2;

	PWRITE(fd, afd, hello, 5, 0, ret1, ret2);
	PWRITE(fd, afd, world, 5, 0, ret1, ret2);

	printf("%d | %d \n", ret1, ret2);

	if (ret1 != ret2)
		return 1;

	char buf1[5], buf2[5];
	READ(fd, afd, buf1, buf2, 5, ret1, ret2);

	printf("%d | %d \n", ret1, ret2);

	printf("%s | %s \n", buf1, buf2);
	if (ret1 != ret2)
		return 1;

	if (strncmp(buf1, buf2, 5))
		return 1;

	return 0;
}

void
run_test(char *name, char *group, TestFunc fn)
{
	if (strncmp(group, group_filter, strlen(group_filter)) == 0) {
		testlist[testcount].name = name;
		testlist[testcount].group = group;

		int result = fn();

		testlist[testcount].status = result;
		testlist[testcount].err = result != 0 ? errno : 0;
		if (result)
			failed++;
		else
			passed++;
		testcount++;

		printf("[%s] [%s] Running Test: %s\n", group, result ? "FAIL" : "PASS", name);
	}
}

int
main(int argc, char *argv[])
{
	printf("Initializing tests.\n");

	imfs_init();

	load_folder("test_folder");

	group_filter = "";
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-g") == 0 && i + 1 < argc)
			group_filter = argv[++i];
	}

	run_test("Valid file.", "open", test_imfs_open_exist);
	run_test("O_CREAT.", "open", test_imfs_open_create);
	run_test("Invalid path.", "open", test_imfs_openclose_invalid);
	run_test("Valid path.", "read", test_read);
	run_test("Valid path.", "write", test_write);
	run_test("dup reads.", "dup", test_dup);
	run_test("pread", "pread", test_pread);
	run_test("pwrite", "pwrite", test_pwrite);

	printf("Passed: %d\n", passed);
	printf("Failed: %d\n", failed);

	if (failed)
		printf("Test\t\tstatus\terrno\n");

	for (int i = 0; i < testcount; i++) {
		struct Test t = testlist[i];
		if (t.status) {
			printf("%s\t%s\t%d\n", t.name, t.status ? "FAIL" : "PASS", t.err);
		}
	}

	return failed;
}

#include <sys/fcntl.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "imfs.h"

#define CAGE_ID	   1

#define OPEN(...)  imfs_open(CAGE_ID, __VA_ARGS__)
#define CLOSE(...) imfs_close(CAGE_ID, __VA_ARGS__)
#define READ(...)  imfs_read(CAGE_ID, __VA_ARGS__)
#define WRITE(...) imfs_write(CAGE_ID, __VA_ARGS__)
#define MKDIR(...) imfs_mkdir(CAGE_ID, __VA_ARGS__)

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
	MKDIR(path, 0);

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

			int imfs_fd = OPEN(fullpath, O_CREAT | O_WRONLY, 0);

			char buffer[1024];
			size_t nread;
			while ((nread = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
				WRITE(imfs_fd, buffer, nread);
			}

			fclose(fp);
			CLOSE(imfs_fd);
		} else {
			load_folder(fullpath);
		}
	}
}

int
test_imfs_open_exist()
{
	int fd = OPEN("/test_folder/hello.txt", O_RDONLY, 0);
	if (fd < 0) {
		perror("");
		return 1;
	}
	if (CLOSE(fd) != 0) {
		perror("");
		return 1;
	}

	return 0;
}

int
test_imfs_open_create()
{
	int fd = OPEN("/test_folder/file_imfs.txt", O_CREAT | O_WRONLY, 0);
	if (fd < 0) {
		return 1;
	}

	if (CLOSE(fd) != 0) {
		return 1;
	}

	return 0;
}

int
test_imfs_openclose_invalid()
{
	int fd = OPEN("/test_folder/file_inexist.txt", O_RDONLY, 0);
	if (fd > 0) {
		return 1;
	}

	return 0;
}

int
test_read()
{
	int fd = OPEN("/test_folder/hello.txt", O_RDONLY, 0);

	if (fd < 0)
		return 1;

	char buf[7];

	READ(fd, buf, 7);

	if (strcmp("hello\n", buf) != 0)
		return 1;

	CLOSE(fd);
	return 0;
}

int
test_write()
{
	char *buf = "hello world";
	int fd = OPEN("/test_folder/file_new.txt", O_CREAT | O_WRONLY, 0);
	if (fd < 0)
		return 1;

	if (WRITE(fd, buf, 12) <= 0)
		return 1;

	CLOSE(fd);

	fd = OPEN("/test_folder/file_new.txt", O_RDONLY, 0);

	char readbuf[12];
	if (READ(fd, readbuf, 12) <= 0)
		return 1;

	if (strcmp(buf, readbuf) != 0)
		return 1;

	CLOSE(fd);

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
}

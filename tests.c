#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/fcntl.h>
#include "imfs.h"

struct Test {
	char *name;
	char *group;
	int status;
	int err;
};

static struct Test testlist[128];
static int testcount = 0, passed=0, failed=0;
static char* group_filter;

typedef int (*TestFunc)(void);

int
test_imfs_open_exist()
{
	int fd = imfs_open("/folder/file1.txt", O_RDONLY, 0);
	if(fd < 0) {
		return 1;
	}
	if(imfs_close(fd) < 0){
		return 1; 
	}

	return 0;
}

int
test_imfs_open_create()
{
	int fd = imfs_open("/folder/file_test.txt",O_CREAT |  O_WRONLY, 0);
	if(fd < 0) {
		return 1;
	}
	if(imfs_close(fd) < 0){
		return 1; 
	}

	return 0;
}

int
test_imfs_openclose_invalid()
{
	int fd = imfs_open("/folder/file_inexist.txt", O_RDONLY, 0);
	if(fd > 0) {
		return 1;
	}

	return 0;
}

int
test_read()
{
	return 0;
}

int
test_read_invalid()
{
	return 0;
}

int
test_write()
{
	return 0;
}

int
test_write_invalid()
{
	return 0;
}

void 
run_test(char *name, char* group, TestFunc fn)
{
	if(strncmp(group, group_filter, strlen(group_filter)) == 0) {
		testlist[testcount].name = name;
		testlist[testcount].group = group;
	
		int result = fn();

		testlist[testcount].status = result;
		testlist[testcount].err = result != 0 ? errno: 0;
		if(result)
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

	imfs_mkdir("/folder", 0);
	imfs_mkdir("/folder/folder_2", 0);

	int fd = imfs_open("/folder/file1.txt", O_CREAT | O_WRONLY, 0);
	imfs_close(fd);

	fd = imfs_open("/folder/folder_2/file2.txt", O_CREAT | O_WRONLY, 0);
	imfs_close(fd);

	group_filter = "";
	for(int i=1; i < argc; i++) {
		if(strcmp(argv[i], "-g") == 0 && i + 1 < argc)
			group_filter = argv[++i];
	}

	run_test("Valid file.", "open", test_imfs_open_exist);
	run_test("O_CREAT.", "open", test_imfs_open_create);
	run_test("Invalid path.", "open", test_imfs_openclose_invalid);
	run_test("Valid path.", "read", test_read);
	run_test("Invalid path.", "read", test_read_invalid);
	run_test("Valid path.", "write", test_write);
	run_test("Invalid path.", "write", test_write_invalid);

	printf("Passed: %d\n", passed);
	printf("Failed: %d\n", failed);

	if(failed)
		printf("Test\t\tstatus\terrno\n");
	
	for(int i=0;i<testcount;i++) {
		struct Test t = testlist[i];
		if(t.status) {
			printf("%s\t%s\t%d\n", t.name, t.status ? "FAIL" : "PASS", t.err);
		}
	}
}

/*
This is an auto-generated file. Do not modify.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <register_handler.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <lind_syscall_num.h>
#include "magic.h"

extern int grate_syscalls[];
extern size_t grate_syscalls_len;

lvar_t l_alloc(int size) {
	char *val = malloc(size);
	return (lvar_t) {
		.value = (uint64_t)val,
		.cage = thiscage,
	};
}

void l_free(lvar_t ptr) {
	free((char *)ptr.value);
}

void register_handlers(int cageid, int grateid) {
	int ret;
	for(int i=0; i < grate_syscalls_len; i++) {
		ret = register_handler(cageid, grate_syscalls[i], 0, grateid);
		printf("Registered: %d\n", grate_syscalls[i]);
	}
}

// Main function will always be same in all grates
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <cage_file> <grate_file> <cage_file> [...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int grateid = getpid();
    
    for (int i = 1; i < (argc < 3 ? argc : 3); i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // will always be cage.
            if (i % 2 != 0) {
                int cageid = getpid();
                
		register_handlers(cageid, grateid);	
	    }

	    printf("Execing %s\n", argv[i]);
            if (execv(argv[i], &argv[i]) == -1) {
                perror("execv failed");
                exit(EXIT_FAILURE);
            }
	} else {
		grate_init();
	}
    }

    int status;
    while (wait(&status) > 0) {
        printf("[Grate | geteuid] terminated, status: %d\n", status);
    }

    return 0;
}

['IN', 'char*', 'pathname']
['N', 'int', 'flags']
['N', 'mode_t', 'mode']
['}']
['']
['N', 'int', 'fd']
['}']
['']
['N', 'int', 'fd']
['OUT', 'void', 'buf[count]']
['N', 'int', 'count']
['}']
['']
['N', 'int', 'fd']
['IN', 'void', 'buf[count]']
['N', 'int', 'count']
['}']
['']
['IN', 'char*', 'pathname']
['N', 'int', 'flags']
['N', 'mode_t', 'mode']
['}']
['']
['N', 'int', 'fd']
['}']
['']
['N', 'int', 'fd']
['OUT', 'void', 'buf[count]']
['N', 'int', 'count']
['}']
['']
['N', 'int', 'fd']
['IN', 'void', 'buf[count]']
['N', 'int', 'count']
['}']
['']

int open_grate(uint64_t cageid, uint64_t arg1, uint64_t arg1cage, uint64_t arg2,
               uint64_t arg2cage, uint64_t arg3, uint64_t arg3cage,
               uint64_t arg4, uint64_t arg4cage, uint64_t arg5,
               uint64_t arg5cage, uint64_t arg6, uint64_t arg6cage) {

  mode_t mode = arg3;

  int flags = arg2;

  char *pathname = malloc(256);

  if (pathname == NULL) {
    perror("malloc failed");
    exit(EXIT_FAILURE);
  }

  cp_data_between_cages(thiscage, arg1cage, arg1, arg1cage, (uint64_t)pathname,
                        thiscage, 256, 1);

  int ret = open_syscall(cageid, pathname, flags, mode);

  return ret;
}


int close_grate(uint64_t cageid, uint64_t arg1, uint64_t arg1cage,
                uint64_t arg2, uint64_t arg2cage, uint64_t arg3,
                uint64_t arg3cage, uint64_t arg4, uint64_t arg4cage,
                uint64_t arg5, uint64_t arg5cage, uint64_t arg6,
                uint64_t arg6cage) {

  int fd = arg1;

  int ret = close_syscall(cageid, fd);

  return ret;
}


int read_grate(uint64_t cageid, uint64_t arg1, uint64_t arg1cage, uint64_t arg2,
               uint64_t arg2cage, uint64_t arg3, uint64_t arg3cage,
               uint64_t arg4, uint64_t arg4cage, uint64_t arg5,
               uint64_t arg5cage, uint64_t arg6, uint64_t arg6cage) {

  int count = arg3;

  void buf = malloc(count);

  if (buf == NULL) {
    perror("malloc failed");
    exit(EXIT_FAILURE);
  }

  int fd = arg1;

  int ret = read_syscall(cageid, fd, buf, count);

  if (arg2 != 0) {
    cp_data_between_cages(thiscage, arg2cage, (uint64_t)buf, thiscage, arg2,
                          arg2cage, count, 0);
  }

  free(buf);

  return ret;
}


int write_grate(uint64_t cageid, uint64_t arg1, uint64_t arg1cage,
                uint64_t arg2, uint64_t arg2cage, uint64_t arg3,
                uint64_t arg3cage, uint64_t arg4, uint64_t arg4cage,
                uint64_t arg5, uint64_t arg5cage, uint64_t arg6,
                uint64_t arg6cage) {

  int count = arg3;

  void buf = malloc(count);

  if (buf == NULL) {
    perror("malloc failed");
    exit(EXIT_FAILURE);
  }

  cp_data_between_cages(thiscage, arg2cage, arg2, arg2cage, (uint64_t)buf,
                        thiscage, count, 0);

  int fd = arg1;

  int ret = write_syscall(cageid, fd, buf, count);

  return ret;
}


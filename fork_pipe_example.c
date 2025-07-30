// TO BUILD: cc -g -o <output> -DLIB [-DDIAG] imfs.c fork_pipe_example.c

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imfs.h"

#define BUF_SIZE	 64
#define TOTAL_WRITES 10

int
main()
{
	imfs_init();

	int pipefd[2];
	int pipefd_child[2];

	pid_t pid;
	char write_buf[BUF_SIZE];
	char read_buf[BUF_SIZE];
	ssize_t bytes_written, bytes_read;
	int i;

	if (imfs_pipe(0, pipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	// Pre fork i.g. This copying is handled by lind.
	imfs_copy_fd_tables(0, 1);
	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid == 0) {
		// Reader
		imfs_close(1, pipefd[1]);
		int total_bytes_read = 0;

		for (i = 0; i < TOTAL_WRITES; i++) {
			size_t expected = strlen("Message #NN: Hello from writer!") + 1;

			bytes_read = imfs_read(1, pipefd[0], read_buf, expected);
			if (bytes_read == -1) {
				perror("read");
				exit(1);
			} else if (bytes_read == 0) {
				fprintf(stderr, "Unexpected EOF\n");
				exit(1);
			}

			printf("Child received: %s\n", read_buf);
			total_bytes_read += bytes_read;
		}
		imfs_close(1, pipefd[0]);
		printf("Child done reading %d bytes\n", total_bytes_read);
		exit(0);
	} else {
		// Writer
		imfs_close(0, pipefd[0]);
		size_t total_written = 0;

		for (i = 0; i < TOTAL_WRITES; i++) {
			snprintf(write_buf, BUF_SIZE, "Message #%02d: Hello from writer!", i + 1);
			size_t len = strlen(write_buf) + 1;

			bytes_written = imfs_write(0, pipefd[1], write_buf, len);
			if (bytes_written == -1) {
				perror("write");
				exit(1);
			}
			total_written += bytes_written;
			printf("Parent sent: %s\n", write_buf);
			sleep(1);
		}
		imfs_close(0, pipefd[1]);
		printf("Parent wrote %d bytes\n", total_written);
		wait(NULL);
		exit(0);
	}
}

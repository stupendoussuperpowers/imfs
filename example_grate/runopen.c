#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <error.h>

int main() {
        printf("[PROG]: Top\n");
        int fd = open("firstfile", O_CREAT | O_WRONLY, 0755);
        printf("[PROG]: FD: %d\n", fd);

	/*
        char *wbuf = "Hello";

        int ret = write(fd, wbuf, 5);
        printf("[PROG] Write: %d\n", ret);

	close(fd);

	fd = open("firstfile", O_RDONLY, 0);

        char rbuf[5];
        ret = read(fd, rbuf, 5);
        printf("[PROG] Read: %d bytes %s\n", ret,  rbuf);

        ret = close(fd);
        printf("[PROG] Close: %d\n", ret);
        */
	return 0;
}

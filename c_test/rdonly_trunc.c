#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>

#include "common.h"

int main(void) {
    int r, fd;
    struct stat sb;

    printf("ex! create\n");
    printf("ex! write\n");
    printf("ex! rdonly_trunc\n");
    printf("ex! stat\n");
    printf("ex! final_size\n");

    unlink("test.file");

    fd  = open("test.file", O_RDWR | O_TRUNC | O_CREAT, 0666);
    check_ok("create", fd);

    r = write(fd, "XXXX", 4);
    check_ok("write", r);

    close(r);

    // rdonly trunc
    fd = open("test.file", O_TRUNC | O_RDONLY);
    check_ok("rdonly_trunc", fd);
    close(fd);

    r = stat("test.file", &sb);
    check_ok("stat", r);

    if (sb.st_size != 4) {
        printf("no! final_size %d, expected 4\n", (int)sb.st_size);
    } else {
        printf("ok! final_size\n");
    }
    return 0;
}

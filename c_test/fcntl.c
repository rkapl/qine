#include "common.h"
#include "stdio.h"
#include <fcntl.h>

int main(void) {
    int fd, r;

    fd = open("fcntl", O_RDONLY);
    check_ok("open", fd);

    r = fcntl(fd, F_GETFD);
    if (r == 0) {
        printf("ok! initial_fcntl\n");
    } else {
        printf("no! initial_fcntl %x\n", r);
    }
    
    r = fcntl(fd, F_SETFD, FD_CLOEXEC);
    check_ok("setfd", r);

    r = fcntl(fd, F_GETFD);
    if (r == FD_CLOEXEC) {
        printf("ok! changed_fcntl\n");
    } else {
        printf("no! changed_fcntl %x\n", r);
    }

    return 0;
}

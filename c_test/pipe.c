#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"

int main(void) {
    int r;
    int fds[2];

    const char *sndbuf = "Hello Pipe!";
    char *rcvbuf[30] = {0};

    printf("ex! pipe\n");
    printf("ex! write\n");
    printf("ex! read\n");
    printf("ex! match\n");

    r = pipe(fds);
    check_ok("pipe", r);

    r = write(fds[1], sndbuf, strlen(sndbuf));
    check_ok("write", r);

    r = read(fds[0], rcvbuf, sizeof(rcvbuf));
    check_ok("read", r);

    if (strcmp(sndbuf, rcvbuf) == 0) {
        printf("ok! match\n");
    } else {
        printf("no! match %s != %s\n", sndbuf, rcvbuf);
    }

    return 0;
}

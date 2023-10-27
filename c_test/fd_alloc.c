#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"

int main(void) {
    int r1, r2;

    printf("ex! open1\n");
    printf("ex! open2\n");
    printf("ex! fdmatch\n");

    touch("test_file");

    r1 = open("test_file", O_RDONLY);
    check_ok("open1", r1);
    close(r1);

    r2 = open("test_file", O_RDONLY);
    check_ok("open2", r2);

    if (r1 == r2) {
        printf("ok! fdmatch\n");
    } else {
        printf("no! fdmatch %d %d\n", r1, r2);
    }

    close(r2);
    return 0;
}

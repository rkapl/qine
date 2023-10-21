#include "common.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

void check_existence(const char *file, const char* msg, int expected) {
    struct stat sb;
    int r = stat(file, &sb);
    int exists;
    if (r != 0) {
        if (errno != ENOENT) {
            perror("stat");
            printf("no! %s unknown error\n", msg);
            return;
        }
    }

    exists = r == 0;
    if (exists == expected) {
        printf("ok! %s exists=%d\n", msg, exists);
    } else {
        printf("no! %s exists=%d\n", msg, exists);
    }
}

void check_ok(const char* msg, int r) {
    if (r >= 0) {
        printf("ok! %s\n", msg);
    } else {
        perror(msg);
        printf("no! %s\n", msg);
    }
}

void touch(const char *file) {
    FILE *f;
    unlink(file);
    f = fopen(file, "w");
    if (!f) {
        perror("touch");
        exit(1);
    }
    fclose(f);
}

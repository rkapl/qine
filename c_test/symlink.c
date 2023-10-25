#include "common.h"

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main(void) {
    const char *test_link = "test.symlink";
    const char *test_file = "test.fil";
    char readlink_path[PATH_MAX + 1];
    struct stat sb;
    int r;

    printf("ex! symlink\n");
    printf("ex! lstat\n");
    printf("ex! symlink_result\n");
    printf("ex! stat_result\n");
    printf("ex! readlink\n");
    printf("ex! readlink_target\n");

    unlink(test_link);
    touch(test_file);

    r = symlink(test_file, test_link);
    check_ok("symlink", r);
    
    r = lstat(test_link, &sb);
    check_ok("lstat", r);
    if (S_ISLNK(sb.st_mode)) {
        printf("ok! symlink_result\n");
    } else {
        printf("no! symlink_result mode=o%o mode=0x%x\n", sb.st_mode, sb.st_mode);
    }

    r = stat(test_link, &sb);
    check_ok("stat", r);

    if (S_ISREG(sb.st_mode)) {
        printf("ok! stat_result\n");
    } else {
        printf("no! stat_result mode=o%o mode=0x%x\n", sb.st_mode, sb.st_mode);
    }

    r = readlink(test_link,readlink_path, sizeof(readlink_path));
    if (r <= 0) {
        perror("readlink");
        printf("no! readlink\n");
    } else {
        printf("ok! readlink\n");
        if (strcmp(readlink_path, test_file) == 0) {
            printf("ok! readlink_target\n");
        } else {
            printf("no! readlink_target %s\n", readlink_path);
        }
    }

    return 0;
}

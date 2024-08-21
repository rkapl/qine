#include "common.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main(void) {
    int r;
    struct stat sb;
    const char *test_dir = "test.dir";
    const char *test_file = "test.file";

    printf("ex! rmdir_file\n");
    printf("ex! rmdir_dir\n");

    mkdir(test_dir, 0777);
    open(test_file, O_CREAT, 0777);

    check_ok("rmdir_dir", rmdir(test_dir));
    if (rmdir(test_file) == 0) {
        printf("no! rmdir_file should not succeed\n");
    } else {
        if (errno != ENOTDIR) {
            printf("no! rmdir_file should return ENOTDIR, not %s\n", strerror(errno));
        } else {
            printf("ok! rmdir_file\n");
        }
    }

    unlink(test_file);

    return 0;
}

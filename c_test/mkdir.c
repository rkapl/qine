#include "common.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    int r;
    struct stat sb;
    const char *test_dir = "test.dir";

    unlink(test_dir);

    r = mkdir(test_dir, 0777);
    check_ok("mkdir", r);

    r = stat(test_dir, &sb);
    check_ok("stat", r);

    if ((sb.st_mode & S_IFDIR) == S_IFDIR) {
        printf("ok! mkdir_result\n");
    } else {
        printf("no! mkdir_result st_mode=0x%o\n", sb.st_mode);
    }

    r = rmdir(test_dir);
    check_ok("rmdir", r);

    return 0;
}

#include "common.h"
#include <unistd.h>
#include <sys/stat.h>

int main(void) {
    const char *f1 = "test.f1";
    const char *f2 = "test.f2";
    struct stat sb;
    int r;

    unlink(f1);
    unlink(f2);

    touch(f1);

    r = link(f1, f2);
    check_ok("link", r);

    r = stat(f2, &sb);
    check_ok("stat", r);

    if (sb.st_nlink == 2) {
        printf("ok! nlink\n");
    } else {
        printf("no! nlink = %d\n", sb.st_nlink);
    }
    return 0;
}

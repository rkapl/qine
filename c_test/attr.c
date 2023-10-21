#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <utime.h>
#include "common.h"

/* Tests various functions from IO and fsys that did not fit elsewhere. 
 * chown, chmod, utime
 */

int main(void) {
    int r;
    FILE *f;
    struct stat sb;
    const char *test_file = "test.file";
    struct utimbuf test_time;

    unlink(test_file);
    fopen(test_file, "w");
    fclose(f);

    r = chown(test_file, 666, 666);
    if (r == 0) {
        printf("ok! chown\n");
    } else {
        if (errno == EPERM) {
            printf("ok! chown\n");
        } else {
            check_ok("chown", r);
        }
    }

    test_time.actime = 123;
    test_time.modtime = 124;
    r = utime(test_file, &test_time);
    check_ok("utime", r);

    r = chmod(test_file, 0111);
    check_ok("chhmod", r);

    r = stat(test_file, &sb);
    if (r != 0) {
        perror("stat");
        return 1;
    }

    sb.st_mode &= 0777;
    if (sb.st_mode == 0111) {
        printf("ok! chmod_result\n");
    } else {
        printf("no! chmod_result = %o, not 111\n", sb.st_mode);
    }

    if (sb.st_atime == 123) {
        printf("ok! atime\n");
    } else {
        printf("no! atime = %u\n", sb.st_atime);
    }

    if (sb.st_mtime == 124) {
        printf("ok! mtime\n");
    } else {
        printf("no! mtime = %u\n", sb.st_mtime);
    }

    return 0;
}

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <utime.h>
#include "common.h"

/* Tests various functions from IO and fsys that did not fit elsewhere. 
 * chown, chmod, utime
 */

int main(void) {
    int r;
    int fd;
    FILE *f;
    struct stat sb;
    const char *test_file = "test.file";
    const char *test_msg = "Hello World\n";
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

    sync();
    printf("ok! sync\n");

    unlink(test_file);
    fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    check_ok("open", fd);
    r = fsync(fd);
    check_ok("fsync", r);

    r = write(fd, test_msg, strlen(test_msg));
    check_ok("write", r);

    r = ltrunc(fd, 4, SEEK_SET);
    check_ok("truncate", r);

    close(fd);

    r = stat(test_file, &sb);
    check_ok("truncate_stat", r);

    if (sb.st_size == 4) {
        printf("ok! truncate_size\n");
    } else {
        printf("no! truncate_size %d\n", (int)sb.st_size);
    }

    return 0;
}

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include "common.h"

int main(void) {
    int r;
    FILE* f = fopen("test.file", "w");
    fclose(f);

    check_existence("test.file", "initial_exists", 1);

    r = rename("test.file", "test.file.2");
    check_ok("rename", r);

    check_existence("test.file.2", "rename_result", 1);
    check_existence("test.file", "rename_result", 0);

    r = unlink("test.file.2");
    check_ok("unlink", r);
    check_existence("test.file.2", "unlink_result", 0);

    return 0;
}

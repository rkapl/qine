#include <unistd.h>
#include <errno.h>
#include <stdio.h>

struct fp_code {
    const char* name;
    int code;
    int can_be_zero;
};

int main(void) {
    struct fp_code fp_codes[] = {
        {"link", _PC_LINK_MAX},
        {"canon", _PC_MAX_CANON},
        {"input", _PC_MAX_INPUT},
        {"name", _PC_NAME_MAX},
        {"path", _PC_PATH_MAX},
        {"pipe_buf", _PC_PIPE_BUF},
        {"chown_restrict", _PC_CHOWN_RESTRICTED, 1},
        {"no_trunc", _PC_NO_TRUNC, 1},
        {NULL, -1}
    };
    int r;

    struct fp_code *fp_code;

    for (fp_code = fp_codes; fp_code->name; fp_code++) {
        r = pathconf("/", fp_code->code);
        if (r < 0) {
            perror(fp_code->name);
            printf("no! %s\n", fp_code->name);
        } else if (r == 0 && !fp_code->can_be_zero) {
            printf("no! %s (zero limit)\n", fp_code->name);
        } else {
            printf("ok! %s = %d\n", fp_code->name, r);
        }
    }

    return 0;
}

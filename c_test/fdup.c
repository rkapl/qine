#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    FILE *new_stdout;
    int r = dup2(1, 10);
    printf("ex! fd\n");
    printf("ex! dup_out\n");
    if (r != 10) {
        perror("dup2");
        printf("no! fd %d\n", r);
    } else {
        const char *msg = "ok! fd_works\n";
        printf("ok! fd\n");
        write(10, msg, strlen(msg));
    }

    r = dup(1);
    printf("dup new fd %d\n", r);
    new_stdout = fdopen(r, "w");
    fprintf(new_stdout, "ok! dup_out\n");

    printf("fff %p\n", fdopen(101, "w"));

    return 0;
}

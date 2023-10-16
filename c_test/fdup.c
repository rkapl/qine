#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    int r = dup2(1, 10);
    if (r != 10) {
        perror("dup2");
        printf("no! fd %d\n", r);
    } else {
        const char *msg = "ok! fd_works\n";
        printf("ok! fd\n");
        write(0, msg, strlen(msg));
    }
    return 0;
}

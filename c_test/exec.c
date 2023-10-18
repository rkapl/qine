#include <stdio.h>
#include <unistd.h>
#include <process.h>

int main (int argc, char **argv) {
    if (argc == 1) {
        int r = fork();
        if (r < 0) {
            printf("no! fork\n");
        } else if (r == 0) {
            printf("ok! child %d\n", r);

            execl("./exec", "1", NULL);
        } else {
            printf("ok! fork %d\n", r);

            wait(NULL);
            printf("wait done\n");
        }
    } else {
        printf("ok! exec_child\n");
    }

    return 0;
}

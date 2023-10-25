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

            execl("./exec", "./exec", "1", NULL);
        } else {
            int pid2;
            printf("ok! fork %d\n", r);

            pid2 = wait(NULL);
            if (pid2 == r) {
                printf("ok! wait\n");
            } else {
                printf("no! wait child = %d\n", pid2);
            }
        }
    } else {
        printf("ok! exec_child\n");
    }

    return 0;
}

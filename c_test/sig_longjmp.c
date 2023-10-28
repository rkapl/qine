#include <stdio.h>
#include <signal.h>
#include <setjmp.h>


static jmp_buf jump;

void sighandler(int _sig) {
    longjmp(jump, 0);
}

int main(void) {
    printf("ex! setjmp\n");
    printf("ex! longjmp\n");

    if (setjmp(jump)) {
        printf("ok! longjmp\n");
        return 0;
    } else {
        printf("ok! setjmp\n");
    }

    signal(SIGUSR1, sighandler);
    raise(SIGUSR1);
    printf("no! longjmp\n");
    return 1;
}

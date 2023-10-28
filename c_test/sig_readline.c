#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// this simulates the timing of signal delivery that readline needs
// it does a "filter signal trick" but first intercepting the signal for itself
// then restoring the original handlers and re-sending it

static void msg(const char *msg) {
    write(2, msg, strlen(msg));
}

static void sig_inner(int _sig);
static void sig_outer(int _sig);

static void sig_outer(int _sig) {
    sigset_t set;

    msg("ok! sig_outer\n");

    signal(SIGUSR1, sig_inner);
    sigprocmask (SIG_BLOCK, (sigset_t *)NULL, &set);
    sigdelset (&set, SIGUSR1);

    kill (getpid (), SIGUSR1);

    /* Let the signal that we just sent through.  */
    sigprocmask (SIG_SETMASK, &set, NULL);

    signal(SIGUSR1, sig_outer);
}

static void sig_inner(int _sig) {
    msg("ok! sig_inner\n");
}

int main(void) {
    printf("sigusr mask %x\n", 1u << SIGUSR1);
    printf("ex! sig_outer\n");
    printf("ex! sig_inner\n");
    printf("ex! done\n");
    fflush(stdout);
    
    signal(SIGUSR1, sig_outer);
    raise(SIGUSR1);

    printf("ok! done\n");
    return 0;
}

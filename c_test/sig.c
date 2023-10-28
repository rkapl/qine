#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

extern void handler(int signo);

static int seen_sig = 0;

int main(void) {
  struct sigaction act;
  act.sa_flags = 0;
  act.sa_mask = 0;
  act.sa_handler = &handler;
  
  printf("ex! sigaction\n");
  printf("ex! signal\n");
  printf("ex! after_signal\n");

  if (sigaction(SIGUSR1, &act, NULL) == 0) {
    printf("ok! sigaction\n");
  } else {
    perror("sigaction");
    printf("no! sigaction\n");
  }

  kill(getpid(), SIGUSR1);

  if (!seen_sig) {
    printf("no! signal not seen\n");
  }

  printf("ok! after_signal\n");
  return 0;
}

void handler(int signo) {
    if (signo == SIGUSR1) {
        printf("ok! signal\n");
    } else {
        printf("no! signal wrong id\n");
    }
    seen_sig = 1;
}

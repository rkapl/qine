#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "common.h"

static void no_handle(int sig) {
    // just interrupt syscall
}

int main(void) {
    pid_t locker;
    int fd1;
    int r;
    struct flock l, ls;

    fd1 = open("lock.test", O_CREAT | O_TRUNC | O_RDWR);
    check_ok("open1", fd1);

    write(fd1, "aaaa", 4);
    lseek(fd1, 0, SEEK_SET);

    ls.l_type = F_WRLCK;
    ls.l_len = 0;
    ls.l_start = 0;
    ls.l_whence = SEEK_SET;
    r = fcntl(fd1, F_GETLK, &ls);
    check_ok("getlk_free", r);

    if (ls.l_type == F_UNLCK) {
        printf("ok! unlocked\n");
    } else {
        perror("F_GETLK");
        printf("no! unlocked %d\n", ls.l_type);
    }

    l.l_type = F_WRLCK;
    l.l_len = 0;
    l.l_start = 0;
    l.l_whence = SEEK_SET;
    r = fcntl(fd1, F_SETLK, &l);
    check_ok("lock1", r);

    fflush(stdout);
    // now fork off so that we can interact with the lock
    locker = fork();

    if (locker != 0) {
        int dummy;
        wait(&dummy);
        return 0;
    }

    ls.l_type = F_WRLCK;
    ls.l_len = 0;
    ls.l_start = 0;
    ls.l_whence = SEEK_SET;
    r = fcntl(fd1, F_GETLK, &ls);
    check_ok("getlk_locked", r);

    if (ls.l_type == F_WRLCK) {
        printf("ok! locked\n");
    } else {
        printf("no! locked %d\n", ls.l_type);
    }

    if (ls.l_start != 0  || ls.l_len != 4) 
        printf("ok! lock_off\n");
    else
        printf("no! lock_off %d +%d\n", ls.l_start, ls.l_len);

    alarm(1);
    signal(SIGALRM, no_handle);

    printf("locking again\n");
    fflush(stdout);
    l.l_type = F_WRLCK;
    l.l_len = 0;
    l.l_start = 0;
    l.l_whence = SEEK_SET;
    r = fcntl(fd1, F_SETLKW, &l);

    if (r == 0 || errno != EINTR) {
        printf("no! lock_wait %d\n", errno);
    } else {
        printf("ok! lock_wait\n");
    }
  
    return 0;
}

#include <stdio.h>
#include <signal.h>
#include <sys/timers.h>
#include <sys/time.h>
#include "common.h"

int main(void) {
    struct itimerspec ms100;
    struct sigevent se = {0};
    int r, timer;

    ms100.it_value.tv_sec = 0;
    ms100.it_value.tv_nsec = 77*1000*1000;
    ms100.it_interval.tv_sec = 0;
    ms100.it_interval.tv_nsec = 78*1000*1000;

    printf("ex! nanosleep\n");
    printf("ex! timer_create\n");


    se.sigev_signo = 0;
    se.sigev_value.sigval_int = 77;
    r = timer_create(CLOCK_REALTIME, &se);
    check_ok("timer_create", r);
    printf("timer id %d\n", r);
    timer = r;

    r = timer_settime(timer, TIMER_AUTO_RELEASE, &ms100, NULL);
    check_ok("settime", r);

    r = sleep(1);
    check_ok("sleep", r);

    return 0;
}

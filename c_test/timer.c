#include <stdio.h>
#include <signal.h>
#include <sys/timers.h>
#include <sys/time.h>
#include "common.h"

int main(void) {
    // Both are just different ways to sleeps. Sleep is implemented internally using the timer.
    struct itimerspec ms100;
    struct sigevent se = {0};
    int r, timer;

    ms100.it_value.tv_sec = 0;
    ms100.it_value.tv_nsec = 250ul*1000ul*1000ul;
    ms100.it_interval.tv_sec = 0;
    ms100.it_interval.tv_nsec = 250ul*1000ul*1000ul;

    printf("ex! sleep\n");
    printf("ex! timer_create\n");
    printf("ex! settime\n");

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

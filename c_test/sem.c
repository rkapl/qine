#include <semaphore.h>
#include <stdio.h>
#include <errno.h>
#include "common.h"

int main(void) {
    sem_t sem;
    int r;

    printf("ex! sem_init\n");
    printf("ex! sem_trywait\n");
    printf("ex! sem_trywait_empty\n");
    printf("ex! sem_post\n");
    printf("ex! sem_wait\n");
    printf("ex! sem_value0\n");
    printf("ex! sem_value1\n");
    printf("ex! sem_destroy\n");

    printf("sem addr %p\n", &sem);

    r = sem_init(&sem, 1, 1);
    check_ok("sem_init", r);

    printf("sem id %d\n", sem.semid);
    printf("sem value %d\n", sem.value);
    fflush(stdout);

    r = sem_trywait(&sem);
    check_ok("sem_trywait", r);

    r = sem_trywait(&sem);
    if (r == -1) {
        if (errno == EAGAIN) {
            printf("ok! sem_trywait_empty\n");
        } else {
            perror("sem_trywait");
            printf("no! sem_trywait_empty\n");
        }
    } else {
        printf("no! sem_trywait_empty returned ok\n");
    }

    r = sem_post(&sem);
    check_ok("sem_ok", r);


    r = sem_wait(&sem);
    check_ok("sem_wait", r);

    if (sem.value == 0) {
        printf("ok! sem_value0\n");
    } else {
        printf("no! sem_value0 %d\n", sem.value);
    }

    sem_post(&sem);
    if (sem.value == 1) {
        printf("ok! sem_value1\n");
    } else {
        printf("no! sem_value1 %d\n", sem.value);
    }

    r = sem_destroy(&sem);
    check_ok("sem_destroy", r);

    printf("\n");

    return 0;
}

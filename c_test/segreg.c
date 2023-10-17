#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

extern void setup_es(uint16_t v);
#pragma aux setup_es = "mov es, ax" modify exact nomemory [] parm [ax];

extern void setup_fs(uint16_t v);
#pragma aux setup_fs = "mov fs, ax" modify exact nomemory [] parm [ax];

extern void setup_gs(uint16_t v);
#pragma aux setup_gs = "mov gs, ax" modify exact nomemory [] parm [ax];

extern uint16_t get_ds(void);
#pragma aux get_ds = "mov ax, ds" modify exact nomemory [ax];

extern uint16_t get_es(void);
#pragma aux get_es = "mov ax, es" modify exact nomemory [ax];

extern uint16_t get_fs(void);
#pragma aux get_fs = "mov ax, fs" modify exact nomemory [ax];

extern uint16_t get_gs(void);
#pragma aux get_gs = "mov ax, gs" modify exact nomemory [ax];

static void handler(int signo) {
    write(0, "signal\n", 7);
}

struct regs {
    uint16_t ds, es, fs, gs;
};

void get_regs(struct regs *r) {
    r->ds = get_ds();
    r->es = get_es();
    r->fs = get_fs();
    r->gs = get_gs();
}

void print_regs(const char *note, struct regs *r) {
    printf("%s: ds %x, es %x, fs %x, gs %x\n", note, r->ds, r->es, r->fs, r->gs);
}

void check_regs(const char *test, struct regs *old, struct regs *now) {
    if (old->ds == now->ds && old->es == now->es && old->fs == now->fs && old->gs == now->gs) {
        printf("ok! %s\n", test);
    } else {
        printf("no! %s\n", test);
    }
}

int main(void) {
    struct regs orig, now;
    uint16_t test_seg;

    get_regs(&orig);
    print_regs("orig", &orig);
    test_seg = qnx_segment_alloc(64);

    setup_es(test_seg);
    setup_gs(test_seg);
    setup_fs(test_seg);
    get_regs(&orig);
    print_regs("setup", &orig);

    write(0, "syscall\n", 8);
    get_regs(&now);
    print_regs("after syscall", &now);
    check_regs("syscall", &orig, &now);

    signal(SIGUSR1, handler);
    kill(getpid(), SIGUSR1);
    print_regs("after signal", &now);
    check_regs("signal", &orig, &now);

    return 0;
}

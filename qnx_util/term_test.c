#include <sys/qnxterm.h>
#include <sys/dev.h>
#include <stdio.h>
#include <unistd.h>

#define N_ATTR TERM_NORMAL

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s -t <termlib input>\n", prog_name);
}

static void termlib_loop();
static void read_loop();

int my_mouse_handler(unsigned *key, struct mouse_event *me) {
    printf("mouse evt %x\r\n", *key);
    if (    (key, me))
        return (1);
    term_mouse_move(-1, -1);
    return (0);
}

int main(int argc, char ** argv) {
    int opt;
    int termlib_flag = 0;
    int mf;

    // Parse command-line options
    while ((opt = getopt(argc, argv, "t")) != -1) {
        switch (opt) {
            case 't':
                termlib_flag = 1;
                break;
            default:
                print_usage(argv[0]);
                return -1;
        }
    }

    // no idea what i am doing with the initialization
    term_load();
    term_setup();
    term_init();
    term_mouse_on();
    mf = TERM_MOUSE_RELEASE | TERM_MOUSE_HELD | TERM_MOUSE_ADJUST | TERM_MOUSE_MENU | TERM_MOUSE_SELECT;
    term_mouse_flags(mf, mf);
    term_mouse_handler(my_mouse_handler);

    printf("term_state qnx_term=%d, scan_mouse=%d, mouse_flags=%x\r\n", term_state.qnx_term, term_state.scan_mouse, term_state.mouse_flags);

    if (termlib_flag) {
        termlib_loop();
    } else {
        read_loop();
    }

    return 0;
}

static void termlib_loop() {
    for (;;) {
        int key = term_key();
        if (key == 4) { // ctrl-d
            return;
        }
        term_printf(1, 1, TERM_NORMAL, "%0x\n", key);
    }
}

static void read_loop() {
        int i, r = 0;
    char c[40];
    char name[5];

    for (;;) {
        term_clear(' ');
        term_printf(1, 1, N_ATTR, "Enter characters, ctrl-D to quit");

        for (i = 0; i < r; i++) {
            if (c[i] < 32) {
                sprintf(name, "^%c", c[i] + 64);
            } else {
                name[0] = c[i];
                name[1] = 0;
            }
            term_printf(i + 2, 3, N_ATTR, "%3s %3d 0x%02x", name, c[i], c[i]);
        }
        term_flush();
        
        dev_read(0, &c[0], 1, 1, 0, 0, 0, 0);
        if (c[0] == 4) {
            // ctrl - d
            return;
        }
        r = dev_read(0, &c[1], sizeof(c) - 1, 0, 0, 3, 0, 0);
        if (r < 0) {
            r = 1;
        } else {
            r++;
        }

    }
}

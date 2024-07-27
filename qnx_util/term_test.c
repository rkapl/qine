#include <sys/qnxterm.h>
#include <sys/dev.h>
#include <stdio.h>

#define N_ATTR TERM_NORMAL

int main() {
    int i, r = 0;
    char c[40];
    char name[5];
    int msgpos;
    printf("TEsn\n");
    // no idea what i am doing with the initialization
    term_load();
    term_setup();
    term_init();
    term_mouse_on();
    term_mouse_flags(TERM_MOUSE_RELEASE | TERM_MOUSE_ADJUST | TERM_MOUSE_MENU | TERM_MOUSE_SELECT, 
                       TERM_MOUSE_RELEASE | TERM_MOUSE_ADJUST | TERM_MOUSE_MENU | TERM_MOUSE_SELECT);
    
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
            return 0;
        }
        r = dev_read(0, &c[1], sizeof(c) - 1, 0, 0, 3, 0, 0);
        if (r < 0) {
            r = 1;
        } else {
            r++;
        }

    }
    
    return 0;
}

#include <stdlib.h>

int main(void) {
    char *r;
    int i;
    for (i = 0; i < 1024*16; i++) {
        r  = malloc(1024);
        r[0] = 0xc;
    }
    printf("ok! malloc\n");
    return 0;
}

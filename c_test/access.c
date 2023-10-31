#include <stdio.h>
#include <unistd.h>

int main(void) {
    int r = access("access", X_OK);
    if (r == 0) {
        printf("ok! access\n");
    } else {
        printf("no! access\n");
    }

    return 0;
}
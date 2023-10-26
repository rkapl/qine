#include <unistd.h>
int main(void) {
    alarm(1);
    pause();
    return 0;
}
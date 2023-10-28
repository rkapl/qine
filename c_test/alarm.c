#include <unistd.h>
#include <signal.h>

void handle_alarm() {

}

int main(void) {
    signal(SIGALRM, handle_alarm);
    alarm(1);
    pause();
    return 0;
}
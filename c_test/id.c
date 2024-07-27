#include <stdio.h>
#include <unistd.h>

int main() {
    printf("getuid %d\n", getuid());
    printf("geteuid %d\n", geteuid());
    printf("getgid %d\n", getgid());
    printf("getegid %d\n", getegid());
    printf("getpid %d\n", getpid());
    printf("getpgid %d\n", getpgid(0));
    return 0;
}

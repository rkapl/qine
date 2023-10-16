#include <stdio.h>
#include <unistd.h>

int main() {
    printf("getuid %d\n", getuid());
    printf("geteuid %d\n", geteuid());
    printf("getgid %d\n", getgid());
    printf("getegid %d\n", getegid());
    return 0;
}
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    int size;
    struct stat sb;
    int r;

    FILE *f = fopen(argv[0], "r");
    if (!f) {
        perror("fopen self");
        printf("no! fopen\n");
    } else {
        printf("ok! fopen\n");
    }

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    printf("size from seek %d\n", size);
    fclose(f);

    r = stat(argv[0], &sb);
    if (r < 0) {
        perror("stat");
        printf("no! stat\n");
    } else {
        printf("ok! stat\n");
    }

    if (size == sb.st_size) {
        printf("ok! stat_size\n");
    } else {
        printf("no! stat_size got %d\n", (int)sb.st_size);
    }

    return 0;
}

#include <stdio.h>
#include <dirent.h>

int main(void) {
    DIR *d;
    struct dirent *e;
    printf("opendir\n");
    d = opendir(".");

    rewinddir(d);

    while((e = readdir(d))) {
        printf("entry: %s\n", e->d_name);
    }

    rewinddir(d);

    while((e = readdir(d))) {
        printf("entry: %s\n", e->d_name);
    }

    closedir(d);

    return 0;
}
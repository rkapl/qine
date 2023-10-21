#include <unistd.h>
#include <stdio.h>
#include <string.h>

static char *dirname(const char *str) {
    const char *last_slash = NULL;
    const char *c;
    char *new_char;
    for (c = str; *c; c++) {
        if (*c == '/')
            last_slash = c;
    }
    if (last_slash == NULL) {
        return NULL;
    }

    /* Make new string and cut it off */
    new_char = strdup(str);
    if (!new_char)
        return NULL;
    new_char[last_slash - str] = 0;
    return new_char;
}

int main(void) {
    const char *cwd_basename;
    char *cwd_dirname;
    char *cwd2;
    char *cwd = getcwd(NULL, 0);
    int r;

    printf("full CWD is %s\n", cwd);
    cwd_basename = basename(cwd);
    cwd_dirname = dirname(cwd);
    if (strcmp(cwd_basename, "cwd") == 0) {
        printf("ok! cwd\n");
    } else {
        printf("no! cwd\n");
    }

    r = chdir("..");
    if (r == 0) {
        printf("ok! chdir\n");
    } else {
        perror("chdir");
        printf("no! chdir\n");
    }
    cwd2 = getcwd(NULL, 0);

    if (strcmp(cwd2, cwd_dirname) == 0) {
        printf("ok! chdir_result\n");
    } else {
        printf("no! chdir_result %s\n", cwd2);
    }

    return 0;
}

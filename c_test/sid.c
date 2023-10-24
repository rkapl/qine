#include <stdio.h>
#include <unistd.h>
#include <sys/sidinfo.h>
#include "common.h"

int main(void) {
    struct _sidinfo sidinfo;
    int sid, sid2, new_sid;

    sid = getsid(getpid());
    check_ok("getsid", sid);
    printf("sid %d\n", sid);

    sid2 = qnx_sid_query(0, sid, &sidinfo);
    check_ok("qnx_sid_query", sid2);

    if (sid2 != sid) {
        printf("no! sid_eq %d %d\n", sid, sid2);
    } else {
        printf("ok! sid_eq\n");
    }

    printf("name: %s tty: %s\n", sidinfo.name, sidinfo.tty_name);

    new_sid = setsid();
    check_ok("setsid", new_sid);
    return 0;
}

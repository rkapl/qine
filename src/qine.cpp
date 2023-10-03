#include <cstdio>
#include <cstdlib>
#include <stdlib.h>
#include <string>
#include <stdio.h>

#include "process.h"
#include "loader.h"

int main(int argc, char **argv) {
    Process::initialize();

    if (!getenv("QNX_ROOT")) {
        fprintf(stderr, "QNX_ROOT not set\n");
        exit(1);
    }
    std::string syslib(getenv("QNX_ROOT"));
    syslib.append("/boot/sys/Slib32");

    load_executable(syslib.c_str(), true);
    load_executable(argv[1], false);

    auto proc = Process::current();
    proc->setup_startup_context(argc - 1, argv + 1);

    proc->enter_emu();
}

#include "process.h"
#include "loader.h"

int main(int argc, char **argv) {
    Process::initialize();

    load_executable("/home/roman/build/qnx/qnxdump/boot/sys/Slib32", true);
    load_executable(argv[1], false);

    auto proc = Process::current();
    proc->setup_startup_context(argc - 1, argv + 1);

    proc->enter_emu();
}
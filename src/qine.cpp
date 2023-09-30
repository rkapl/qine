#include "process.h"
#include "loader.h"

int main(int argc, char **argv) {
    Process::initialize();

    load_executable("/home/roman/build/qnx/qnxdump/boot/sys/Slib32", true);
    load_executable("/home/roman/build/qnx/qnxdump/bin/echo", false);

    Process::current()->enter_emu();
}
#include "process.h"
#include "loader.h"

int main(int argc, char **argv) {
    Process::initialize();

    load_executable(argv[1]);
}
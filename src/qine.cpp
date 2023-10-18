#include <stdlib.h>
#include <string>
#include <stdio.h>
#include <getopt.h>
#include <vector>

#include "process.h"
#include "loader.h"
#include "log.h"

static struct option cmd_options[] = {
    {0}
};

static void handle_log_opt(const char *opt) {
    bool enable = true;
    if (*opt == '+') {
        enable = true;
        opt++;
    } else if  (*opt == '-') {
        enable = false;
        opt++;
    }

    Log::Category c = Log::by_name(opt);
    if (c == Log::INVALID) {
        fprintf(stderr, "Invalid log category: %s", opt);
        exit(1);
    }

    Log::enable(c, enable);
}

static void handle_help() {
    printf("qine [options] executable [args]\n");
}

int main(int argc, char **argv) {
    for (;;) {
        const char* opts = "d:h";
        int c = getopt_long(argc, argv, opts, cmd_options, NULL);
        if (c == -1)
            break;
        switch(c) {
            case '?':
                handle_help();
                exit(1);
                break;
            case 'h':
                handle_help();
                exit(0);
                break;
            case 'd':
                handle_log_opt(optarg);
                break;
            default:
                fprintf(stderr, "getopt unknown code 0x%x\n", c);
                exit(1);
        }
    }

    std::vector<std::string> self_call;
    for (int i = 0; i < optind; i++) {
        self_call.push_back(argv[i]);
    }
    
    argc -= optind;
    argv += optind;

    Process::initialize(std::move(self_call));

    if (!getenv("QNX_ROOT")) {
        fprintf(stderr, "QNX_ROOT not set\n");
        exit(1);
    }
    std::string syslib(getenv("QNX_ROOT"));
    syslib.append("/boot/sys/Slib32");

    load_executable(syslib.c_str(), true);

    load_executable(argv[0], false);

    auto proc = Process::current();
    proc->setup_startup_context(argc, argv);

    proc->enter_emu();
}

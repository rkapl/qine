#include <stdlib.h>
#include <string>
#include <stdio.h>
#include <getopt.h>
#include <filesystem>
#include <vector>

#include "process.h"
#include "loader.h"
#include "log.h"

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
        fprintf(stderr, "Invalid log category: %s\n", opt);
        exit(1);
    }

    Log::enable(c, enable);
}

static void handle_help() {
    printf("qine [options] executable [args]\n");
}

static struct option cmd_options[] = {
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'},
    {"map", no_argument, 0, 'm'},
};

int main(int argc, char **argv) {
    auto proc = Process::create();
    try {
        for (;;) {
            const char* opts = "d:h:r:m:";
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
                case 'm':
                    proc->path_mapper().add_map(optarg);
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
        self_call[0] = std::filesystem::absolute(self_call[0]);
        
        argc -= optind;
        argv += optind;

        proc->initialize(std::move(self_call));
    } catch (const ConfigurationError& e) {
        fprintf(stderr, "%s\n", e.m_msg.c_str());
        return 1;
    }

    if (argc == 0) {
        fprintf(stderr, "program arguments expected\n");
        return 1;
    }

    std::string slib_path;
    if (getenv("QNX_SLIB")) {
        slib_path = getenv("QNX_SLIB");
    } else {
        auto p = PathInfo::mk_qnx_path("/boot/sys/Slib32");
        proc->path_mapper().map_path_to_host(p);
        slib_path = p.host_path();
    }
    load_executable(slib_path.c_str(), true);

    auto exec_path = PathInfo::mk_qnx_path(argv[0]);
    proc->path_mapper().map_path_to_host(exec_path);
    load_executable(exec_path.host_path(), false);

    proc->setup_startup_context(argc, argv);

    proc->enter_emu();
}

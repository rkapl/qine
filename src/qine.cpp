#include <functional>
#include <stdlib.h>
#include <string>
#include <stdio.h>
#include <getopt.h>
#include <filesystem>
#include <vector>

#include "fsutil.h"
#include "process.h"
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


int opt_no_slib;

namespace Opt {
    enum {
        EXEC = 200,
    };
}

static struct option cmd_options[] = {
    {"help", no_argument, 0, 'h'},
    {"debug", required_argument, 0, 'd'},
    {"map", required_argument, 0, 'm'},
    {"lib", required_argument, 0, 'l'},
    {"no-slib", no_argument, &opt_no_slib, 1},
    {"exec", required_argument, 0, Opt::EXEC},
};


int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IOLBF, 256);
    setvbuf(stderr, nullptr, _IOLBF, 256);
    auto proc = Process::create();

    // we only want to set debugs settings and maps during init, then do the rest
    std::vector<std::function<void()>> delayed_args;
    std::string opt_exec;

    try {
        for (;;) {
            const char* opts = "d:hr:m:s:l:";
            int c = getopt_long(argc, argv, opts, cmd_options, NULL);
            const char* this_optarg = optarg;
            if (c == -1)
                break;
            switch(c) {
                case 0:
                    // flag set instead
                    break;
                case '?':
                    handle_help();
                    exit(1);
                    break;
                case 'l':
                    delayed_args.push_back([proc, this_optarg] {proc->load_library(this_optarg); });
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
                case Opt::EXEC:
                    opt_exec = optarg;
                    break;
                default:
                    fprintf(stderr, "getopt unknown code 0x%x\n", c);
                    exit(1);
            }
        }

        proc->initialize_2();

        /* Remember all the arguments in case Qine needs to exec itself (to run another QNX binary) */
        std::vector<std::string> self_call;
        for (int i = 0; i < optind; i++) {
            if (strcmp(argv[i], "--") == 0)
                continue;
            self_call.push_back(argv[i]);
        }
        self_call[0] = std::filesystem::absolute(self_call[0]);
        
        argc -= optind;
        argv += optind;

        proc->initialize_self_call(std::move(self_call));
    } catch (const ConfigurationError& e) {
        fprintf(stderr, "%s\n", e.m_msg.c_str());
        return 1;
    }

    if (argc == 0 && opt_exec.empty()) {
        fprintf(stderr, "program arguments expected\n");
        return 1;
    }

    PathInfo exec_path;
    if (!opt_exec.empty()) {
        exec_path = proc->path_mapper().map_path_to_qnx(opt_exec.c_str());
    } else if (Fsutil::is_abs(argv[0])) {
        exec_path = proc->path_mapper().map_path_to_host(argv[0]);
    } else {
        std::string abspath;
        Fsutil::realpath(argv[0], abspath);
        exec_path = proc->path_mapper().map_path_to_qnx(abspath.c_str());
    }
    proc->load_executable(exec_path);

    /* loading of libraries*/
    for (auto d: delayed_args)
        d();

    if (!proc->slib_loaded() && !opt_no_slib) {
        fprintf(stderr, "No suitable system library specified on commandline using --lib. "
            "For most executables, this needs to be specified. Use --no-slib to override\n");
        exit(1);
    }

    proc->setup_startup_context(argc, argv);

    proc->enter_emu();
}

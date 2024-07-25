#pragma once

#include "types.h"
#include "path_mapper.h"

struct InterpreterInfo {
    InterpreterInfo(): has_interpreter(false) {}
    bool has_interpreter;
    PathInfo interpreter;
    std::string interpreter_args;
    PathInfo original_executable;
};

class LoaderFormatException: public std::runtime_error {
public:
    LoaderFormatException(const char *description): std::runtime_error(description) {}
};

struct LoadInfo {
    LoadInfo(): b16(false), entry(0), ss(0), ds(0), cs(0), heap_start(0), stack_low(0), stack_size(0) {}

    bool b16;
    uint32_t entry;
    uint16_t ss, ds, cs;
    uint32_t heap_start;
    uint32_t stack_low;
    uint32_t stack_size;
};

void loader_check_interpreter(int fd, InterpreterInfo *interp_out);
void loader_load(int fd, LoadInfo *info_out, bool slib);
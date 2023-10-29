#pragma once

#include "types.h"

struct LoadInfo {
    LoadInfo(): entry(0), ss(0), ds(0), cs(0), heap_start(0), stack_low(0), stack_size(0) {}

    uint32_t entry;
    uint16_t ss, ds, cs;
    uint32_t heap_start;
    uint32_t stack_low;
    uint32_t stack_size;
};

void loader_load(const char* path, LoadInfo *info_out, bool slib);
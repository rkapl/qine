#pragma once

#include "types.h"

struct LoadInfo {
    SelectorId code;
    uint32_t offset;
    SelectorId data;
    SelectorId stack;
    SelectorId aux;
};

void load_executable(const char* path, bool slib);
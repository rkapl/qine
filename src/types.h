#pragma once

#include <stdint.h>

using SelectorId = uint16_t;

/* must match the QNX exec format */
enum class Access {
    READ_WRITE = 0,
    READ_ONLY = 1,
    EXEC_READ = 2,
    EXEC_ONLY = 3,

    INVALID = 8,
};

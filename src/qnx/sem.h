#pragma once

#include "types.h"

namespace Qnx {
    struct sem_t {
        uint32_t value;
        uint16_t semid;
    };
}
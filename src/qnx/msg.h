#pragma once

#include "types.h"
#include <cstdint>
#include <stdint.h>

namespace Qnx {

struct MsgHeader {
    uint16_t type;
    uint16_t subtype;
};

struct MsgReplyHeader {
    uint16_t status;
};

}
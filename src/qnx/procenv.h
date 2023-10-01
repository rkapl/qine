#pragma once

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include "../types.h"
#include "../compiler.h"

namespace Qnx {
    using Sigset = uint32_t;
    constexpr size_t SIG_COUNT = 32;

    struct Sigaction {
        uint32_t handler_fn;
        Sigset mask;
        uint16_t flags;
    } qine_attribute_packed;

    /* This layout is described in the SIN manual */
    struct Sigtab {
        uint16_t unknown_1;
        uint16_t unknown_2;
        Sigaction actions[SIG_COUNT];
    } qine_attribute_packed;

    /* Reverse-enginnered from Slib32 */
    struct ProcessEnvironment {
        Sigtab sigtab;
        char cwd[0x100];
        char root_prefix[0x100];
    };

    static_assert(offsetof(ProcessEnvironment, cwd) == 0x144);
    static_assert(offsetof(ProcessEnvironment, root_prefix) == 0x244);
}
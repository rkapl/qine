#pragma once

#include <stdint.h>

namespace MemOps {
    // always x86
    constexpr int PAGE_SIZE = 4096;

    static inline uintptr_t align_up(uintptr_t v, uintptr_t align) {
        if (v % align == 0) {
            return v;
        }
        return (v / align + 1) * align;
    }

    static inline uintptr_t align_down(uintptr_t v, uintptr_t align) {
        return (v / align) * align;
    }

    static inline uintptr_t align_page_up(uintptr_t v) {
        return align_up(v, PAGE_SIZE);
    }

    static inline uintptr_t align_page_down(uintptr_t v) {
        return align_down(v, PAGE_SIZE);
    }

    static inline uintptr_t kilo(uintptr_t v) {
        return v * 1024;
    }

    static inline uintptr_t mega(uintptr_t v) {
        return v * 1024 * 1024;
    }
}
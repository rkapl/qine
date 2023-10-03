#pragma once

#include <cstdint>
#include <stdint.h>

using SegmentId = uint16_t;
using GuestPtr = uint32_t;

enum class RwOp {
    READ = 0,
    WRITE = 1,
};

/* must match the QNX exec format */
enum class Access {
    READ_WRITE = 0,
    READ_ONLY = 1,
    EXEC_READ = 2,
    EXEC_ONLY = 3,

    INVALID = 8,
};

struct FarPointer {
    FarPointer() = default;
    constexpr FarPointer(uint16_t segment, uint32_t offset): m_segment(segment), m_offset(offset) {}

    /* Yes, the typical storage of far pointers seem to be the other way around */
    uint32_t m_offset;
    /* This is selector, not segment id! */
    uint16_t m_segment;

    static constexpr FarPointer null(){ return FarPointer(0, 0); }
};


struct FarSlice {
    FarSlice() = default;
    constexpr FarSlice(FarPointer ptr, uint32_t size): m_ptr(ptr), m_size(size) {}

    FarPointer m_ptr;
    uint32_t m_size;

    bool is_empty() const {return m_size == 0;}
};
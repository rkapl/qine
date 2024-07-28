#pragma once

#include <cstdint>
#include <memory>
#include "types.h"
#include "cpp.h"

class Segment;

/* One segment descriptor in the LDT*/
class SegmentDescriptor {
public:
    SegmentDescriptor(SegmentDescriptor&&) = default;
    SegmentDescriptor& operator=(SegmentDescriptor&&) = default;
    SegmentDescriptor(SegmentId id, Access access, const std::shared_ptr<Segment>& seg, Bitness bits);
    ~SegmentDescriptor();
    
    static constexpr uint16_t SEL_LDT = 1u << 2;

    static constexpr uint16_t mk_sel(SegmentId id);
    static constexpr uint16_t mk_invalid_sel(int id);
    static constexpr SegmentId sel_to_id(uint16_t id);

    inline uint16_t id() const;
    inline uint16_t selector() const;
    inline FarPointer pointer(uint32_t offset);
    inline void change_access(Access a) { m_access = a;}
    inline Access access() const {return m_access;}
    const std::shared_ptr<Segment> segment() const { return m_seg; }

    /* Call needed if underlying descriptor changes */
    void update_descriptors();
private:
    // Because only one id can exist at a time to manage the LDT space
    NoCopy m_no_copy;

    void remove_descriptors();

    SegmentId m_id;
    Access m_access;
    std::shared_ptr<Segment> m_seg;
    Bitness m_bits;
};

constexpr uint16_t SegmentDescriptor::mk_sel(SegmentId id){
    /* 16-bit QNX uses ring 1 for root program, interestingly  */
    return (id << 3) | SEL_LDT | 3;
}
constexpr SegmentId SegmentDescriptor::sel_to_id(uint16_t id) {
    return id >> 3;
}

uint16_t SegmentDescriptor::selector() const {
    return mk_sel(m_id);
}

uint16_t SegmentDescriptor::id() const {
    return m_id;
}

FarPointer SegmentDescriptor::pointer(uint32_t offset) {
    return FarPointer(selector(), offset);
}

constexpr uint16_t SegmentDescriptor::mk_invalid_sel(int id) {
    // just made up selector value as a sentinel for debugging
    return 0x100 + (id << 3);
}
#pragma once

#include <cstdint>
#include <memory>
#include "types.h"
#include "cpp.h"

class Segment;

class SegmentDescriptor {
public:
    SegmentDescriptor() noexcept = default;
    SegmentDescriptor(SegmentDescriptor&&) = default;
    SegmentDescriptor& operator=(SegmentDescriptor&&) = default;
    SegmentDescriptor(SegmentId id, Access access, const std::shared_ptr<Segment>& seg);
    ~SegmentDescriptor();
    
    static constexpr uint16_t SEL_LDT = 1u << 2;
    static constexpr uint16_t SEL_RPL3 = 3u;

    static constexpr uint16_t mk_sel(SegmentId id);
    static constexpr uint16_t mk_invalid_sel(int id);
    static constexpr SegmentId sel_to_id(uint16_t id);

    inline uint16_t id() const;
    inline uint16_t selector() const;
    inline FarPointer pointer(uint32_t offset);
    const std::shared_ptr<Segment> segment() const { return m_seg; }

    /* Call neede if underlying descriptor changes */
    void update_descriptors();
private:
    // Because only one id can exist at a time to manage the LDT space
    NoCopy m_no_copy;
    void remove_descriptors();

    SegmentId m_id;
    Access m_access;
    std::shared_ptr<Segment> m_seg;
};

constexpr uint16_t SegmentDescriptor::mk_sel(SegmentId id){
    return (id << 3) | SEL_LDT | SEL_RPL3;
}
constexpr SegmentId SegmentDescriptor::sel_to_id(uint16_t id) {
    return id >> 3;
}

uint16_t SegmentDescriptor::selector() const {
    return (m_id << 3) | SEL_LDT | SEL_RPL3;
}

uint16_t SegmentDescriptor::id() const {
    return m_id;
}

FarPointer SegmentDescriptor::pointer(uint32_t offset) {
    return FarPointer(selector(), offset);
}

constexpr uint16_t SegmentDescriptor::mk_invalid_sel(int id) {
    return 0x100 + (id << 3);
}
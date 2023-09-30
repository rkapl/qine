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
    SegmentDescriptor(SelectorId id, Access access, const std::shared_ptr<Segment>& seg);
    ~SegmentDescriptor();

    inline uint16_t selector() const;
    const std::shared_ptr<Segment> segment() const { return m_seg; }
private:
    static constexpr uint16_t SEL_LDT = 1u << 2;
    static constexpr uint16_t SEL_RPL3 = 3u;
    // Because only one id can exist at a time to manage the LDT space
    NoCopy m_no_copy;
    void update_descriptors();
    void remove_descriptors();

    SelectorId m_id;
    Access m_access;
    std::shared_ptr<Segment> m_seg;
};

uint16_t SegmentDescriptor::selector() const {
    return (m_id << 3) | SEL_LDT | SEL_RPL3;
}
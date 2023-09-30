#include <cstddef>
#include "process.h"
#include "emu.h"
#include "segment.h"
#include "segment_descriptor.h"
#include "context.h"

Process* Process::m_current = nullptr;

Process::Process(): m_segment_descriptors(1024)
{

}

Process::~Process() {}

void Process::initialize() {
    assert(!m_current);
    m_current = new Process();
    m_current->m_emu.init();
    memset(&m_current->startup_context, 0xcc, sizeof(m_current->startup_context));
}

void Process::enter_emu()
{
    m_emu.enter_emu();
}

void* Process::translate_segmented(uint16_t selector, uint32_t offset, uint32_t size, bool write)
{
    auto sd = m_segment_descriptors.get(selector >> 3);
    if (!sd)
        throw GuestStateException("segment not present");
    
    if (!sd->segment()->check_bounds(offset, size))
        throw GuestStateException("address out of segment bounds");

    // TODO: write check not implemented
    
    return sd->segment()->pointer(offset, size);
}

std::shared_ptr<Segment> Process::allocate_segment() {
    auto seg = std::make_shared<Segment>();
    m_segments.add_front(seg.get());
    return seg;
}

SegmentDescriptor* Process::create_segment_descriptor(Access access, const std::shared_ptr<Segment>& mem)
{
    auto sd = m_segment_descriptors.alloc();
    auto idx = m_segment_descriptors.index_of(sd);
    *sd = std::move(SegmentDescriptor(idx, access, mem));
    return sd;
}
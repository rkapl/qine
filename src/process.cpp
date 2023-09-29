#include "process.h"
#include "segment.h"

Process* Process::m_current = nullptr;

Process::Process() {}

Process::~Process() {}

void Process::initialize() {
    assert(!m_current);
    m_current = new Process();
}

Segment *Process::allocate_segment(SegmentId index) {
    if (index >= m_segments.size()) {
        m_segments.resize(index + 1);
    }

    auto& slot = m_segments[index];
    assert(slot == nullptr);
    slot = std::make_unique<Segment>(index);
    return slot.get();
}

Segment *Process::get_segment(SegmentId index)
{
    if (index >= m_segments.size())
        return nullptr;
    return m_segments[index].get();
}
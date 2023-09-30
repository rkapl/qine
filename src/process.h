#pragma once

#include <assert.h>
#include <cstdint>
#include <optional>
#include <stddef.h>
#include <sys/ucontext.h>
#include <vector>
#include <memory>
#include <ucontext.h>

#include "cpp.h"
#include "emu.h"
#include "types.h"
#include "intrusive_list.h"
#include "idmap.h"
#include "segment_descriptor.h"

class Segment;

/* Represents the currently running process, a singleton */
class Process{
public:
    static inline Process* current();
    static void initialize();

    std::shared_ptr<Segment> allocate_segment();
    SegmentDescriptor* create_segment_descriptor(Access access, const std::shared_ptr<Segment>& mem);
    void enter_emu();

    void* translate_segmented(uint16_t selector, uint32_t offset, uint32_t size, bool write);

    /* Only the mcontext will actually be used*/
    ucontext_t startup_context;
private:
    NoCopy m_mark_nc;
    NoMove m_mark_nm;

    Process();
    ~Process();
    static Process* m_current;

    IntrusiveList::List<Segment> m_segments;
    IdMap<SegmentDescriptor> m_segment_descriptors;
    Emu m_emu;
};

Process* Process::current() {
    assert(m_current);
    return m_current;
}
#pragma once

#include <assert.h>
#include <cstdint>
#include <optional>
#include <stddef.h>
#include <sys/ucontext.h>
#include <vector>
#include <memory>
#include <ucontext.h>

#include "context.h"
#include "cpp.h"
#include "emu.h"
#include "qnx/magic.h"
#include "qnx/procenv.h"
#include "qnx/types.h"
#include "segment.h"
#include "types.h"
#include "intrusive_list.h"
#include "idmap.h"
#include "segment_descriptor.h"

class Segment;

struct LoadInfo {
    std::optional<FarPointer> entry_main;
    std::optional<FarPointer> entry_slib;
    std::optional<SegmentId> data_segment;
};

/* Represents the currently running process, a singleton */
class Process{
    friend class Emu;
    friend class Context;
public:
    static inline Process* current();
    static void initialize();

    std::shared_ptr<Segment> allocate_segment();
    SegmentDescriptor* create_segment_descriptor(Access access, const std::shared_ptr<Segment>& mem);
    SegmentDescriptor* create_segment_descriptor_at(Access access, const std::shared_ptr<Segment>& mem, SegmentId id);

    void setup_startup_context(int argc, char **argv);
    void enter_emu();

    void* translate_segmented(FarPointer ptr, uint32_t size = 0, RwOp op = RwOp::READ);

    Qnx::pid_t pid();
    Qnx::pid_t parent_pid();

    void set_errno(int v);

    /* Information modified by loader */
    /* Only the mcontext will actually be used.*/
    ucontext_t m_startup_context_main;
    ExtraContext m_startup_context_extra;
    Context m_startup_context;

    LoadInfo m_load;
private:
    NoCopy m_mark_nc;
    NoMove m_mark_nm;

    Process();
    ~Process();

    SegmentDescriptor* descriptor_by_selector(uint16_t id);
    void setup_magic(SegmentDescriptor *data_sd, SegmentAllocator& alloc);

    static Process* m_current;

    IntrusiveList::List<Segment> m_segments;
    IdMap<SegmentDescriptor> m_segment_descriptors;

    std::shared_ptr<Segment> m_magic_pointer;
    FarPointer m_magic_guest_pointer;
    Qnx::Magic* m_magic;

    Emu m_emu;
};

Process* Process::current() {
    assert(m_current);
    return m_current;
}
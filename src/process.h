#pragma once

#include <assert.h>
#include <cstdint>
#include <optional>
#include <stddef.h>
#include <sys/ucontext.h>
#include <vector>
#include <memory>
#include <map>
#include <ucontext.h>

#include "context.h"
#include "cpp.h"
#include "emu.h"
#include "process_fd.h"
#include "main_handler.h"
#include "msg_handler.h"
#include "qnx/magic.h"
#include "qnx/procenv.h"
#include "qnx/types.h"
#include "segment.h"
#include "types.h"
#include "intrusive_list.h"
#include "idmap.h"
#include "segment_descriptor.h"

class Segment;
class MsgInfo;

class CwdTooLong: public std::runtime_error {
public:
    CwdTooLong(): std::runtime_error("working directory path too long for QNX") {}
};

struct LoadInfo {
    std::optional<FarPointer> entry_main;
    std::optional<FarPointer> entry_slib;
    std::optional<SegmentId> data_segment;
};

/* Represents the currently running process, a singleton */
class Process{
    friend class Emu;
    friend class Context;
    friend class MainHandler;
public:
    static inline Process* current();
    static void initialize();

    std::shared_ptr<Segment> allocate_segment();
    SegmentDescriptor* create_segment_descriptor(Access access, const std::shared_ptr<Segment>& mem);
    SegmentDescriptor* create_segment_descriptor_at(Access access, const std::shared_ptr<Segment>& mem, SegmentId id);
    void* translate_segmented(FarPointer ptr, uint32_t size = 0, RwOp op = RwOp::READ);

    void setup_startup_context(int argc, char **argv);
    void enter_emu();

    void handle_msg(MsgInfo& m);

    Qnx::pid_t pid() const;
    Qnx::pid_t parent_pid() const;
    Qnx::nid_t nid() const;
    const std::string& file_name() const;

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

    ProcessFd *fd_get(int id);
    void fd_release(int id);

    static Process* m_current;

    IntrusiveList::List<Segment> m_segments;
    IdMap<SegmentDescriptor> m_segment_descriptors;
    
    std::map<int, ProcessFd> m_fds;
    MainHandler m_main_handler;

    std::shared_ptr<Segment> m_magic_pointer;
    FarPointer m_magic_guest_pointer;
    Qnx::Magic* m_magic;

    Emu m_emu;
    std::string m_file_name;
};

Process* Process::current() {
    assert(m_current);
    return m_current;
}
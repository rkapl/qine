#pragma once

#include <assert.h>
#include <cstdint>
#include <optional>
#include <stddef.h>
#include <string>
#include <sys/ucontext.h>
#include <vector>
#include <memory>
#include <map>
#include <ucontext.h>

#include "guest_context.h"
#include "cpp.h"
#include "emu.h"
#include "main_handler.h"
#include "msg_handler.h"
#include "path_mapper.h"
#include "qnx/magic.h"
#include "qnx/procenv.h"
#include "qnx/types.h"
#include "segment.h"
#include "types.h"
#include "intrusive_list.h"
#include "idmap.h"
#include "qnx_fd.h"
#include "qnx_pid.h"
#include "segment_descriptor.h"
#include "loader.h"

class Segment;
class MsgContext;


/* Represents the currently running process, a singleton */
class Process{
    friend class Emu;
    friend class GuestContext;
    friend class MainHandler;
public:
    static inline Process* current();
    static Process* create();
    void initialize_self_call(std::vector<std::string>&& self_call);

    void update_pids_after_fork(pid_t new_pid);

    std::shared_ptr<Segment> allocate_segment();
    SegmentDescriptor* create_segment_descriptor(Access access, const std::shared_ptr<Segment>& mem);
    SegmentDescriptor* create_segment_descriptor_at(Access access, const std::shared_ptr<Segment>& mem, SegmentId id);
    void* translate_segmented(FarPointer ptr, uint32_t size = 0, RwOp op = RwOp::READ);
    FdMap& fds() {return m_fds;}
    PidMap& pids() {return m_pids;}
    PathMapper& path_mapper() {return m_path_mapper;}

    void setup_startup_context(int argc, char **argv);
    void enter_emu();

    void handle_msg(MsgContext& m);

    Qnx::pid_t pid() const;
    Qnx::pid_t parent_pid() const;
    Qnx::nid_t nid() const;

    void load_library(std::string_view load_arg);
    // Path must be resolved in both host & qnx
    void load_executable(const PathInfo& path);
    bool slib_loaded() const { return m_slib_entry != 0; }
    const std::vector<std::string>& self_call() const;
    const PathInfo& executed_file() const;

    void set_errno(int v);

private:
    NoCopy m_mark_nc;
    NoMove m_mark_nm;

    Process();
    ~Process();

    SegmentDescriptor* descriptor_by_selector(uint16_t id);
    void setup_magic(SegmentDescriptor *data_sd, StartupSbrk& alloc);
    void initialize();
    void initialize_pids();

    static Process* m_current;

    // loader
    ucontext_t m_startup_context_main;
    ExtraContext m_startup_context_extra;
    GuestContext m_startup_context;
    InterpreterInfo m_interpreter_info;
    LoadInfo m_load_exec;
    LoadInfo m_load_slib;
    uint32_t m_slib_entry;

    // memory
    IntrusiveList::List<Segment> m_segments;
    IdMap<SegmentDescriptor> m_segment_descriptors;
    
    // "outsourced" components
    Emu m_emu;
    FdMap m_fds;
    MainHandler m_main_handler;
    PathMapper m_path_mapper;

    // pids
    PidMap m_pids;
    QnxPid* m_my_pid;
    QnxPid* m_parent_pid;

    // Qnx special memory areas
    std::shared_ptr<Segment> m_magic_pointer;
    FarPointer m_magic_guest_pointer;
    Qnx::Magic *m_magic;

    std::shared_ptr<Segment> m_time_segment;
    SegmentId m_time_segment_selector;

    Qnx::Sigtab *m_sigtab;

    // Self info
    PathInfo m_executed_file;
    std::vector<std::string> m_self_call;
};

Process* Process::current() {
    assert(m_current);
    return m_current;
}
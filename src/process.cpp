#include <algorithm>
#include <stdexcept>
#include <string>
#include <sys/ucontext.h>
#include <system_error>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

#include "gen_msg/dev.h"
#include "gen_msg/fsys.h"
#include "gen_msg/io.h"
#include "gen_msg/proc.h"
#include "loader.h"
#include "msg/meta.h"
#include "msg/dump.h"
#include "path_mapper.h"
#include "qnx/magic.h"
#include "qnx/msg.h"
#include "qnx/procenv.h"
#include "qnx/types.h"

#include "mem_ops.h"
#include "process.h"
#include "emu.h"
#include "msg_handler.h"
#include "segment.h"
#include "segment_descriptor.h"
#include "guest_context.h"
#include "types.h"
#include "log.h"
#include "util.h"
#include "fsutil.h"
#include "cmd_opts.h"

Process* Process::m_current = nullptr;

Process::Process(): 
    m_segment_descriptors(1024),
    m_sigtab(nullptr),
    m_fds(),
    m_magic_guest_pointer(FarPointer::null()),
    m_slib_entry(0)
{
}

Process::~Process() {}

Process* Process::create() {
    assert(!m_current);
    m_current = new Process();
    m_current->initialize();
    return m_current;
}

void Process::initialize() {
    m_startup_context = GuestContext(&m_startup_context_main, &m_startup_context_extra);
    memset(&m_startup_context_main, 0xcc, sizeof(m_startup_context_main));
    memset(&m_startup_context_extra, 0xcc, sizeof(m_startup_context_extra));
}

void Process::initialize_2() {
    m_emu.init();
    m_fds.scan_host_fds(m_current->nid(), 1, 1);
    initialize_pids();
}

void Process::initialize_self_call(std::vector<std::string>&& self_call) {
    m_self_call = std::move(self_call);
}

void Process::initialize_pids() {
    // reserve the "proc" (QNX central process) pid
    pid_t self = getpid();
    pid_t ppid = getppid();
    m_pids.alloc_permanent_pid(QnxPid::PID_PROC, -1);
    m_pids.alloc_permanent_pid(QnxPid::PID_UNKNOWN, -1);
    m_parent_pid = m_pids.alloc_permanent_pid(QnxPid::PID_ROOT_PARENT, ppid);

    m_my_pid = m_pids.alloc_related_pid(self, QnxPid::Type::SELF);

    m_pids.alloc_related_pid(getsid(ppid), QnxPid::Type::SID);
    m_pids.alloc_related_pid(getpgid(ppid), QnxPid::Type::PGID);

    m_pids.alloc_related_pid(getsid(self), QnxPid::Type::SID);
    m_pids.alloc_related_pid(getpgid(self), QnxPid::Type::PGID);
}

void Process::load_library(std::string_view load_arg) {
    std::string lib;
    uint32_t entry;
    namespace CO = CommandOptions;
    bool slib = false;
    CO::parse(load_arg, {
        .core = {
            new CO::String(&lib)
        },
        .kwargs = {
            new CO::KwArg<CO::Integer<uint32_t>>("entry", &entry),
            new CO::KwArg<CO::Flag>("sys", &slib),
        }
    });

    if (slib && entry == 0) {
        throw ConfigurationError("Entry point must be specified for slib");
    }

    LoadInfo li;
    Log::print(Log::LOADER, "loading library %s\n", lib.c_str());
    UniqueFd fd(open(lib.c_str(), O_RDONLY));
    if (!fd.valid()) {
        perror("loader open");
        throw LoaderFormatException("Failed to open library for loading");
    }
    loader_load(fd.get(), &li, true);

    if (slib) {
        m_slib_entry = entry;
        m_load_slib = li;
    }
}

void Process::load_executable(const PathInfo &path) {
    const PathInfo *current_path = &path;

    Log::print(Log::LOADER, "loading executable %s\n", path.host_path());
    UniqueFd fd(open(path.host_path(), O_RDONLY));
    if (!fd.valid()) {
        perror("loader open");
        throw LoaderFormatException("Failed to open executable for loading");
    }

    loader_check_interpreter(fd.get(), &m_interpreter_info);
    if (m_interpreter_info.has_interpreter) {
        m_interpreter_info.original_executable = path;
        path_mapper().map_path_to_host(m_interpreter_info.interpreter);
        Log::print(Log::LOADER, "loading interpreter %s\n", m_interpreter_info.interpreter.host_path());
        current_path = &m_interpreter_info.interpreter;
    }

    fd = UniqueFd(open(current_path->host_path(), O_RDONLY));
        if (!fd.valid()) {
        perror("interpreter open");
        throw LoaderFormatException("Failed to open interpreter for loading");
    }
    loader_load(fd.get(), &m_load_exec, false);
    std::string realpath;
    if (!Fsutil::realpath(current_path->host_path(), realpath)) {
        throw std::system_error(errno, std::system_category());
    }
    m_executed_file = path_mapper().map_path_to_qnx(realpath.c_str());
}

void Process::update_pids_after_fork(pid_t new_pid) {
    m_parent_pid = m_my_pid;
    m_my_pid = m_pids.alloc_related_pid(new_pid, QnxPid::Type::SELF);
    m_magic->my_pid = pid();
    m_magic->dads_pid = parent_pid();
    m_magic->my_nid = nid();
    // printf("Updated PIDs: parent: qnx=%d host=%d\n", m_parent_pid->qnx_pid(), m_parent_pid->host_pid());
    // printf("Updated PIDs: self: qnx=%d host=%d\n", m_my_pid->qnx_pid(), m_my_pid->host_pid());
}

Qnx::pid_t Process::pid() const
{
    return m_my_pid->qnx_pid();
}

Qnx::pid_t Process::parent_pid() const
{
    return m_parent_pid->qnx_pid();
}

Qnx::pid_t Process::nid() const
{
    return 0x1;
}
const PathInfo& Process::executed_file() const {
    return m_executed_file;
}

const std::vector<std::string>& Process::self_call() const {
    return m_self_call;
}

void Process::enter_emu()
{
    m_emu.enter_emu();
}

void* Process::translate_segmented(FarPointer ptr, uint32_t size, RwOp write)
{
    auto sd = descriptor_by_selector(ptr.m_segment);
    if (!sd)
        throw SegmentationFault("segment not present");
    
    if (!sd->segment()->check_bounds(ptr.m_offset, size))
        throw SegmentationFault("address out of segment bounds");

    // TODO: write check not implemented
    
    return sd->segment()->pointer(ptr.m_offset, size);
}

std::shared_ptr<Segment> Process::allocate_segment() {
    auto seg = std::make_shared<Segment>();
    m_segments.add_front(seg.get());
    return seg;
}

SegmentDescriptor* Process::create_segment_descriptor(Access access, const std::shared_ptr<Segment>& mem)
{
    return m_segment_descriptors.alloc_any([=](size_t idx) {return new SegmentDescriptor(idx, access, mem);});
}

SegmentDescriptor* Process::create_segment_descriptor_at(Access access, const std::shared_ptr<Segment>& mem, SegmentId id)
{
    return m_segment_descriptors.alloc_exactly_at(id, [=](size_t idx) {return new SegmentDescriptor(idx, access, mem);});
}

SegmentDescriptor* Process::descriptor_by_selector(uint16_t id) {
    return m_segment_descriptors[id >> 3];
}

void Process::set_errno(int v) {
    m_magic->Errno = v;
}

void Process::setup_magic(SegmentDescriptor *data_sd, StartupSbrk& alloc)
{
     /* Allocate place for the magic */
    alloc.alloc(sizeof(Qnx::Magic));
    m_magic_guest_pointer = data_sd->pointer(alloc.offset());
    m_magic = reinterpret_cast<Qnx::Magic*>(alloc.ptr());
    
    /* Now create the magical segment that will be pointing to magic */
    m_magic_pointer = allocate_segment();
    m_magic_pointer->reserve(MemOps::PAGE_SIZE);
    m_magic_pointer->grow(Access::READ_WRITE, MemOps::PAGE_SIZE);
    m_magic_pointer->set_limit(8);
    m_magic_pointer->make_shared();
    // It is swapped for som reason
    *reinterpret_cast<FarPointer*>(m_magic_pointer->pointer(0, sizeof(FarPointer))) = m_magic_guest_pointer;
    /* And publish it under well known ID. Unfortunately, we cannot publish GDT selectors (understandably), 
     * so we will fake it in emu 
     */
    create_segment_descriptor_at(Access::READ_ONLY, m_magic_pointer, SegmentDescriptor::sel_to_id(Qnx::MAGIC_PTR_SELECTOR));

    for (size_t i = 0; i < sizeof(*m_magic) / 4; i++) {
        // this helps us identify the values we filled out wront down the line
        *(reinterpret_cast<uint32_t*>(m_magic) + i) = 0xDEADBE00 + i;
    }

    m_magic->my_pid = pid();
    m_magic->dads_pid = parent_pid();
    m_magic->my_nid = nid();

}

void Process::handle_msg(MsgContext& m)
{
    auto& msg = m.msg();
    Qnx::MsgHeader hdr;
    msg.read_type(&hdr);

    bool msg_searched = false;
    const Meta::Message *msg_type = nullptr;

    auto find_msg = [hdr, &msg, &msg_type, &msg_searched] {
        if (msg_searched) {
            return;
        }

        const Meta::MessageList *ml = nullptr;
        msg_searched = true;
        uint8_t msg_class = hdr.type >> 8;
        switch (msg_class) {
            case 0: 
                ml = &QnxMsg::proc::list;
                break;
            case 1: 
                ml = &QnxMsg::io::list;
                break;
            case 2:
                ml = &QnxMsg::fsys::list;
                break;
            case 3:
                ml = &QnxMsg::dev::list;
                break;
            default:
                fprintf(stderr, "Msg receiver unknown, cannot dump\n");
                break;
        }
        if (ml)
            msg_type = Meta::find_message(stdout, *ml, hdr);
    };

    Log::if_enabled(Log::MSG, [&](FILE *s) {
        find_msg();
        if (msg_type) {
            if (m.m_via_fd) {
                fprintf(s, "request %s fd %d {\n", msg_type->m_name, m.m_fd);
            } else {
                fprintf(s, "request %s pid %d {\n", msg_type->m_name, m.m_pid);
            }
            Meta::dump_structure(s, *msg_type->m_request, msg);
            fprintf(s, "}\n");
        } else {
            msg.dump_send(s);
        }
    });

    // meat of the function
    m_main_handler.receive(m);

    Log::if_enabled(Log::MSG_REPLY, [&](FILE *s) {
        find_msg();
        if (msg_type) {
            fprintf(s, "reply %s {\n", msg_type->m_name);
            Meta::dump_structure_written(s, *msg_type->m_reply, msg);
            fprintf(s, "}\n");
        }
    });
}

void Process::setup_startup_context(int argc, char **argv)
{
    auto& ctx = m_startup_context;
    if (m_load_exec.entry == 0 || m_load_exec.cs == 0 || m_load_exec.ds == 0) {
        throw GuestStateException("Loading incomplete");
    }

    ctx.reg_ss() = m_load_exec.ss;
    ctx.reg_cs() = m_load_exec.cs;
    ctx.reg_ds() = m_load_exec.ds;
    ctx.reg_es() = m_load_exec.ds;
    ctx.reg_fs() = m_load_exec.ds;

    ctx.reg_esp() = m_load_exec.stack_low + m_load_exec.stack_size - 4;
    ctx.reg_edx() = m_load_exec.stack_low;

    auto data_sd = descriptor_by_selector(m_load_exec.ss);
    if (!data_sd) {
        throw GuestStateException("Guest does not seem to be loaded properly (no data segment)");
    }

    auto data_seg = data_sd->segment();
    auto alloc = StartupSbrk(data_seg.get(), m_load_exec.heap_start);
    setup_magic(data_sd, alloc);

    /* Create time segment so that we do not crash, but we do not fill the time yet */
    m_time_segment = allocate_segment();
    m_time_segment->reserve(MemOps::PAGE_SIZE);
    m_time_segment->grow(Access::READ_ONLY, MemOps::PAGE_SIZE);
    m_time_segment->set_limit(0x20);

    m_time_segment_selector =  create_segment_descriptor(Access::READ_ONLY, m_time_segment)->id();

    /* Now create the stack environment, that is argc, argv, arge  */
    ctx.push_stack(0); // Unknown
    ctx.push_stack(nid()); // nid?
    ctx.push_stack(parent_pid());
    ctx.push_stack(pid());

    /* Environment */
    ctx.push_stack(0);
    auto push_env = [&](const char *c) {
        //printf("pushing env %s\n", c);
        alloc.push_string(c);    
        ctx.push_stack(alloc.offset());
    };

    // cmd and pfx must be last (probably so that the runtime can easily remove them)
    std::string env_cwd;
    std::string env_pfx;
    const char *env_cwd_key = "__CWD=";
    const char *env_pfx_key = "__PFX=";

    for (char **e = environ; *e; e++) {
        if (starts_with(*e, env_cwd_key))
            env_cwd = *e;
        if (starts_with(*e,  env_pfx_key))
            env_pfx = *e;
    }

    if (env_cwd.empty()) {
        std::string host_cwd(Fsutil::getcwd());
        auto cwd_path = path_mapper().map_path_to_qnx(host_cwd.c_str());
        env_cwd.append(env_cwd_key);
        env_cwd.append(cwd_path.qnx_path());
    }
    push_env(env_cwd.c_str());

    if (!env_pfx.empty())
        push_env(env_pfx.c_str());
    
    for (char **e = environ; *e; e++) {
        if (starts_with(*e, env_cwd_key))
            continue;
        if (starts_with(*e,  env_pfx_key))
            continue;
        //printf("Passing %s\n", *e);
        push_env(*e);
    }

    /* Argv in reverse order*/
    ctx.push_stack(0);
    int final_argc = 0;
    auto push_arg = [&](const char *arg) {
        final_argc++;
        alloc.push_string(arg);
        ctx.push_stack(alloc.offset());
    };
    for (int i = argc - 1; i > 0; i--) {
        push_arg(argv[i]);
    }
    if (!m_interpreter_info.has_interpreter) {
        push_arg(argv[0]);
    } else {
        if (!m_interpreter_info.interpreter_args.empty())
            push_arg(m_interpreter_info.interpreter_args.c_str());
        push_arg(m_interpreter_info.original_executable.qnx_path());
        push_arg(m_interpreter_info.interpreter.qnx_path());
    }

    /* Argc */
    ctx.push_stack(final_argc);

    /* Entry points*/
    if (!slib_loaded()) {
        ctx.reg_cs() = m_load_exec.cs;
        ctx.reg_eip() = m_load_exec.entry;
        Log::print(Log::LOADER, "Program start: %x:%x\n", m_load_exec.cs, m_load_exec.entry);
    } else {
        ctx.reg_cs() = m_load_slib.cs;
        ctx.reg_eip() = m_slib_entry;
        auto e = &m_load_exec.entry;
        ctx.push_stack(m_load_exec.cs);
        ctx.push_stack(m_load_exec.entry);
        Log::print(Log::LOADER, "Slib start: %x:%x\n", m_load_slib.cs, m_slib_entry);
        Log::print(Log::LOADER, "Program start: %x:%x\n", m_load_exec.cs, m_load_exec.entry);
    }

    data_sd->update_descriptors();

    /* What we have allocated (curbrk)*/
    ctx.reg_ebx() = alloc.next_offset();
    /* What remains free */    
    ctx.reg_ecx() = data_seg->size() - alloc.next_offset();

    Log::if_enabled(Log::LOADER, [&](FILE *s) {
        fprintf(s, "Stack %x:%x - %x, esp %x, aux data %x:%x, heap: %x:%x, free_heap: %x\n",
             ctx.reg_ss(), m_load_exec.stack_low, m_load_exec.stack_low + m_load_exec.stack_size, ctx.reg_esp(),
             ctx.reg_ds(), m_load_exec.heap_start,
             ctx.reg_ds(), ctx.reg_ebx(), ctx.reg_ecx()
        );
    });

    // ctx.dump(stdout);

    // breakpoints
    //ctx.write<uint8_t>(Context::CS, 0x61e , 0xCC);
}
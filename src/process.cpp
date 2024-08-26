#include <algorithm>
#include <stdexcept>
#include <string>
#include <sys/ucontext.h>
#include <system_error>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

#include "fd_filter.h"
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
#include "qnx/osinfo.h"
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
    m_slib_entry(0),
    m_bits(B32)
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
    m_fds.scan_host_fds(m_current->nid(), 1, 1);
    m_emu.init();
    initialize_pids();
}

void Process::attach_term_emu() {
    try {
        auto fd = m_fds.get_open_fd(0);
        if (isatty(fd->m_host_fd)) {
            fd->m_filter = std::make_unique<TerminalFilter>();
            setenv("TERM", "xterm-qine1", 1);
        }
    } catch (const BadFdException&) {
        // just silently don't attach terminal filter
    }
}

void Process::initialize_2() {
}

void Process::initialize_self_call(std::vector<std::string>&& self_call) {
    m_self_call = std::move(self_call);
    m_self_call[0] = Fsutil::proc_main_exe();
}

void Process::initialize_pids() {
    // reserve the "proc" (QNX central process) pid
    pid_t self = getpid();
    pid_t ppid = getppid();
    m_pids.alloc_permanent_pid(QnxPid::PID_PROC, -1);
    m_pids.alloc_permanent_pid(QnxPid::PID_UNKNOWN, -1);

    m_parent_pid = m_pids.alloc_related_pid(ppid, QnxPid::Type::ROOT_PARENT);
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
    bool slib16 = false;
    CO::parse(load_arg, {
        .core = {
            new CO::String(&lib)
        },
        .kwargs = {
            new CO::KwArg<CO::Integer<uint32_t>>("entry", &entry),
            new CO::KwArg<CO::Flag>("sys", &slib),
            new CO::KwArg<CO::Flag>("sys16", &slib16),
        }
    });

    if ((slib || slib16) && entry == 0) {
        throw ConfigurationError("Entry point must be specified for slib");
    }

    // load only the slib we need
    if (slib && m_bits == B16) {
        return;
    }
    if (slib16 && m_bits == B32) {
        return;
    }

    LoadInfo li;
    Log::print(Log::LOADER, "loading library %s\n", lib.c_str());
    UniqueFd fd(open(lib.c_str(), O_RDONLY));
    if (!fd.valid()) {
        perror("loader open");
        throw LoaderFormatException("Failed to open library for loading");
    }
    loader_load(fd.get(), &li, true);

    if (slib || slib16) {
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
        perror("executable open");
        throw LoaderFormatException("Failed to open executable for loading");
    }
    loader_load(fd.get(), &m_load_exec, false);
    std::string realpath;
    if (!Fsutil::realpath(current_path->host_path(), realpath)) {
        throw std::system_error(errno, std::system_category());
    }
    m_bits = m_load_exec.bits;
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

void Process::debug_dump_segments() {
    auto& sdmap = m_current->m_segment_descriptors;
    int i = 0;
    while ((i = (sdmap.search(i, true))) != IdMap<SegmentDescriptor>::INVAL) {
        auto sd = sdmap[i];
        fprintf(stderr, "sel %x: size %04zx, access %x, %08x,\n", SegmentDescriptor::mk_sel(sd->id()), 
            sd->segment()->size(), (int)sd->access(), sd->segment()->location());
        i++;
    }
}

std::shared_ptr<Segment> Process::allocate_segment() {
    auto seg = std::make_shared<Segment>();
    m_segments.add_front(seg.get());
    return seg;
}

SegmentDescriptor* Process::create_segment_descriptor(Access access, const std::shared_ptr<Segment>& mem, Bitness bits)
{
    return m_segment_descriptors.alloc_any([=](size_t idx) {return new SegmentDescriptor(idx, access, mem, bits);});
}

SegmentDescriptor* Process::create_segment_descriptor_at(Access access, const std::shared_ptr<Segment>& mem, Bitness bits, SegmentId id)
{
    return m_segment_descriptors.alloc_exactly_at(id, [=](size_t idx) {return new SegmentDescriptor(idx, access, mem, bits);});
}

SegmentDescriptor* Process::descriptor_by_selector(uint16_t id) {
    return m_segment_descriptors[id >> 3];
}

void Process::free_segment_descriptor(SegmentDescriptor *sd) {
    m_segment_descriptors.free(sd->id());
}

void Process::set_errno(int v) {
    m_magic->Errno = v;
}

void Process::setup_magic(SegmentDescriptor *data_sd, StartupSbrk& alloc)
{
     /* Allocate place for the magic */
    alloc.align();
    alloc.alloc(sizeof(Qnx::Magic));
    m_magic_guest_pointer = data_sd->pointer(alloc.offset());
    m_magic = reinterpret_cast<Qnx::Magic*>(alloc.ptr());
    
    /* Now create the magical segment that will be pointing to magic */
    m_magic_pointer = allocate_segment();
    m_magic_pointer->reserve(MemOps::PAGE_SIZE);
    m_magic_pointer->grow_bytes(8);
    m_magic_pointer->make_shared();
    *reinterpret_cast<FarPointer*>(m_magic_pointer->pointer(0, sizeof(FarPointer))) = m_magic_guest_pointer;
    
    /* And publish it under well known ID. Unfortunately, we cannot publish GDT selectors (understandably), 
     * so we will fake it in emu 
     */
    create_segment_descriptor_at(Access::READ_ONLY, m_magic_pointer, B32, SegmentDescriptor::sel_to_id(Qnx::MAGIC_PTR_SELECTOR));

    for (size_t i = 0; i < sizeof(*m_magic) / 4; i++) {
        // this helps us identify the values we filled out wrong down the line
        *(reinterpret_cast<uint32_t*>(m_magic) + i) = 0xDEADBE00 + i;
    }

    m_magic->my_pid = pid();
    m_magic->dads_pid = parent_pid();
    m_magic->my_nid = nid();
}

void Process::setup_magic16(SegmentDescriptor *data_sd, StartupSbrk& alloc)
{
     /* Allocate place for the magic */
    alloc.align();
    alloc.alloc(sizeof(Qnx::Magic16));
    m_magic_guest_pointer = data_sd->pointer(alloc.offset());
    m_magic16 = reinterpret_cast<Qnx::Magic16*>(alloc.ptr());
    
    /* Now create the magical segment that will be pointing to magic */
    m_magic_pointer = allocate_segment();
    m_magic_pointer->reserve(MemOps::PAGE_SIZE);
    m_magic_pointer->grow_bytes(6);
    *reinterpret_cast<FarPointer16*>(m_magic_pointer->pointer(0, sizeof(FarPointer))) = FarPointer16(m_magic_guest_pointer);
    
    /* And publish it under well known ID. Unfortunately, we cannot publish GDT selectors (understandably), 
     * so we will fake it in emu 
     */
    create_segment_descriptor_at(Access::READ_ONLY, m_magic_pointer, B32, SegmentDescriptor::sel_to_id(Qnx::MAGIC_PTR_SELECTOR));

    for (size_t i = 0; i < sizeof(*m_magic16) / 4; i++) {
        // this helps us identify the values we filled out wrong down the line
        m_magic16->sptrs[i].m_segment = 0xDEA0 + i;
    }

    m_magic16->my_pid = pid();
    m_magic16->dads_pid = parent_pid();
    m_magic16->my_nid = nid();
    m_magic16->sptrs[Qnx::SPTRS16_SLIB16PTR] = FarPointer16(m_load_slib.cs, m_slib_entry);
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

void Process::push_pointer_block(const std::vector<GuestPtr>& block) {
    auto& ctx = m_startup_context;
    ctx.push_stack(0);
    for (size_t i = 0; i < block.size(); i++) {
        ctx.push_stack(block[block.size() - i - 1]);
    }
}

void Process::update_timesel() {
    auto t = reinterpret_cast<Qnx::timesel*>(m_time_segment->pointer(0, sizeof(Qnx::timesel)));
    Timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    t->cnt8254 = 0;
    t->seconds = now.tv_sec;
    t->nsec = now.tv_nsec;
    t->nsec_inc = 0;
    t->cycles_per_sec = 1000;
    int64_t cycles = static_cast<int64_t>(t->seconds) * 1000 + static_cast<int64_t>(now.tv_nsec) / 1000 / 1000;
    t->cycle_lo = cycles;
    t->cycle_hi = cycles >> 32;
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

    ctx.reg_esp() = m_load_exec.stack_low + m_load_exec.stack_size;
    ctx.reg_edx() = m_load_exec.stack_low;
    ctx.reg_ebp() = pid();

    auto data_sd = descriptor_by_selector(m_load_exec.ss);
    if (!data_sd) {
        throw GuestStateException("Guest does not seem to be loaded properly (no data segment)");
    }

    auto data_seg = data_sd->segment();
    auto alloc = StartupSbrk(data_seg.get(), m_load_exec.heap_start);
    if (m_bits == B16) {
        setup_magic16(data_sd, alloc);
    } else {
        setup_magic(data_sd, alloc);
    }

    /* Create time segment so that we do not crash, but we do not fill the time yet */
    // TODO: make the segment non-accessible and enable signal if some is reading it
    m_time_segment = allocate_segment();
    m_time_segment->reserve(MemOps::PAGE_SIZE);
    m_time_segment->grow_bytes(sizeof(Qnx::timesel));

    m_time_segment_selector =  create_segment_descriptor(Access::READ_ONLY, m_time_segment, B32)->id();

    /* Now create spawn message and the stack environment, that is argc, argv, arge  */
    /* in 32bit mode, the stack is prepared and there are more things passed on stack as "arguments",
      including the argument and environment lists
      in 16bit mode, there are just registeres and the magic segment and the code manages the stack itself
    */
    bool stack_env = m_bits == B32;
    alloc.align();
    alloc.alloc(sizeof(QnxMsg::proc::spawn));
    ctx.reg_edi() = alloc.offset();
    auto spawn = reinterpret_cast<QnxMsg::proc::spawn*>(alloc.ptr());
    spawn->m_type = QnxMsg::proc::msg_spawn::TYPE;

    std::vector<GuestPtr> argv_offsets;
    std::vector<GuestPtr> envp_offsets;

    if (m_interpreter_info.has_interpreter) {
        alloc.push_string(m_interpreter_info.original_executable.qnx_path());
    } else {
        alloc.push_string(argv[0]);
    }

    /* Append argv after the spawn message */
    auto push_arg = [&](const char *c) {
        // printf("Adding arg '%s'\n", c);
        alloc.push_string(c);
        argv_offsets.push_back(alloc.offset());
    };

     if (!m_interpreter_info.has_interpreter) {
        push_arg(argv[0]);
    } else {
        push_arg(m_interpreter_info.interpreter.qnx_path());
        if (!m_interpreter_info.interpreter_args.empty())
            push_arg(m_interpreter_info.interpreter_args.c_str());
        push_arg(m_interpreter_info.original_executable.qnx_path());        
    }

    /* push all normal args except argv[0] */
    for (int i = 1; i < argc; i++) {
        push_arg(argv[i]);
    }
    alloc.push_string("");

    /* Append environment after the spawn message */
    auto push_env = [&](const char *c) {
        // printf("pushing env %s\n", c);
        alloc.push_string(c);
        envp_offsets.push_back(alloc.offset());
    };

    // cmd and pfx must be first (probably so that the runtime can easily remove them)
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

    for (char **e = environ; *e; e++) {
        if (starts_with(*e, env_cwd_key))
            continue;
        if (starts_with(*e,  env_pfx_key))
            continue;
        push_env(*e);
    }

    if (!env_pfx.empty()) {
        push_env(env_pfx.c_str());
    } else {
        push_env("__PFX=//1");
    }

    if (env_cwd.empty()) {
        std::string host_cwd(Fsutil::getcwd());
        auto cwd_path = path_mapper().map_path_to_qnx(host_cwd.c_str());
        env_cwd.append(env_cwd_key);
        env_cwd.append(cwd_path.qnx_path());
    }
    push_env(env_cwd.c_str());
    alloc.push_string("");

    /* Finalize the spawn message */
    spawn->m_argc = argv_offsets.size();
    spawn->m_envc = envp_offsets.size();
    alloc.align();
    
    /* Generate the stack env for 32bit code*/
    if (stack_env) {
        ctx.push_stack(0); // Unknown
        ctx.push_stack(nid()); // nid?
        ctx.push_stack(parent_pid());
        ctx.push_stack(pid());

        push_pointer_block(envp_offsets);
        push_pointer_block(argv_offsets);
        ctx.push_stack(argv_offsets.size());
    }

    /* Entry points*/
    if (!slib_loaded() || m_bits == B16) {
        ctx.reg_cs() = m_load_exec.cs;
        ctx.reg_eip() = m_load_exec.entry;
        Log::print(Log::LOADER, "Program start: %x:%x\n", m_load_exec.cs, m_load_exec.entry);
    } else {
        ctx.reg_cs() = m_load_slib.cs;
        ctx.reg_eip() = m_slib_entry;
        auto e = &m_load_exec.entry;
        if (stack_env) {
            ctx.push_stack(m_load_exec.cs);
            ctx.push_stack(m_load_exec.entry);
        }
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
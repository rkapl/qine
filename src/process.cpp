#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <bits/types/FILE.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/ucontext.h>
#include <system_error>

#include "gen_msg/dev.h"
#include "gen_msg/fsys.h"
#include "gen_msg/io.h"
#include "gen_msg/proc.h"
#include "msg/meta.h"
#include "msg/dump.h"
#include "process_fd.h"
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
#include "context.h"
#include "types.h"
#include "log.h"

Process* Process::m_current = nullptr;

Process::Process(): 
    m_segment_descriptors(1024),
    m_sigtab(nullptr),
    m_fds(),
    m_magic_guest_pointer(FarPointer::null())
{
}

Process::~Process() {}

void Process::initialize() {
    assert(!m_current);
    m_current = new Process();
    m_current->m_startup_context = Context(&m_current->m_startup_context_main, &m_current->m_startup_context_extra);
    memset(&m_current->m_startup_context_main, 0xcc, sizeof(m_current->m_startup_context_main));
    memset(&m_current->m_startup_context_extra, 0xcc, sizeof(m_current->m_startup_context_extra));
    m_current->m_emu.init();
}

Qnx::pid_t Process::pid() const
{
    return 0x1001;
}

Qnx::pid_t Process::parent_pid() const
{
    return 0x1002;
}

Qnx::pid_t Process::nid() const
{
    return 0x1;
}

const std::string& Process::file_name() const {
    return m_file_name;
}

void Process::enter_emu()
{
    m_emu.enter_emu();
}

void* Process::translate_segmented(FarPointer ptr, uint32_t size, RwOp write)
{
    auto sd = descriptor_by_selector(ptr.m_segment);
    if (!sd)
        throw GuestStateException("segment not present");
    
    if (!sd->segment()->check_bounds(ptr.m_offset, size))
        throw GuestStateException("address out of segment bounds");

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
    auto sd = m_segment_descriptors.alloc();
    auto idx = m_segment_descriptors.index_of(sd);
    *sd = std::move(SegmentDescriptor(idx, access, mem));
    return sd;
}

SegmentDescriptor* Process::create_segment_descriptor_at(Access access, const std::shared_ptr<Segment>& mem, SegmentId id)
{
    auto sd = m_segment_descriptors.alloc_at(id);
    *sd  = std::move(SegmentDescriptor(id, access, mem));
    return sd;
}

SegmentDescriptor* Process::descriptor_by_selector(uint16_t id) {
    return m_segment_descriptors.get(id >> 3);
}

void Process::set_errno(int v) {
    m_magic->Errno = v;
}

void Process::setup_magic(SegmentDescriptor *data_sd, SegmentAllocator& alloc)
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

void Process::handle_msg(MsgInfo& m)
{
    auto& msg = m.msg();
    Qnx::MsgHeader hdr;
    msg.read_type(&hdr);

    // most of this functio is loggin
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
                printf("Msg receiver unknown, cannot dump\n");
                break;
        }
        if (ml)
            msg_type = Meta::find_message(stdout, *ml, msg);
    };

    Log::if_enabled(Log::MSG, [&](FILE *s) {
        find_msg();
        if (msg_type) {
            if (m.m_via_fd) {
                fprintf(stdout, "request %s fd %d {\n", msg_type->m_name, m.m_fd);
            } else {
                fprintf(stdout, "request %s pid %d {\n", msg_type->m_name, m.m_pid);
            }
            Meta::dump_structure(s, *msg_type->m_request, msg);
            fprintf(stdout, "}\n");
        } else {
            msg.dump_send(s);
        }
    });

    // meat of the function
    // TODO: allow dispatching to individual handler
    m_main_handler.receive(m);

    Log::if_enabled(Log::MSG_REPLY, [&](FILE *s) {
        find_msg();
        if (msg_type) {
            fprintf(stdout, "reply %s {\n", msg_type->m_name);
            Meta::dump_structure_written(s, *msg_type->m_reply, msg);
            fprintf(stdout, "}\n");
        }
    });
}

static std::string cpp_getcwd() {
    std::string s;
    s.resize(Qnx::QPATH_MAX_T);
    if (getcwd(s.data(), s.length()) != NULL) {
        return s;
    } else {
        if (errno == ERANGE || errno == ENAMETOOLONG) {
            throw CwdTooLong();
        }
        throw std::logic_error("getcwd failed");
    }
}

void Process::setup_startup_context(int argc, char **argv)
{
    auto& ctx = m_startup_context;
    if (!m_load.entry_main.has_value() || ! m_load.entry_slib.has_value() || !m_load.data_segment.has_value()) {
        throw GuestStateException("Loading incomplete");
    }

    /* First allocate some space in data segment */
    auto data_sd = m_segment_descriptors.get(m_load.data_segment.value());
    if (!data_sd) {
        throw GuestStateException("Guest does not seem to be loaded properly (no data segment)");
    }

    auto data_seg = data_sd->segment();
    auto alloc = SegmentAllocator(data_seg.get());

    setup_magic(data_sd, alloc);

    /* Now create the stack environment, that is argc, argv, arge  */
    ctx.push_stack(0); // Unknown
    ctx.push_stack(nid()); // nid?
    ctx.push_stack(parent_pid());
    ctx.push_stack(pid());

    /* Environment */
    ctx.push_stack(0);

    auto push_env = [&](const char *c) {
        alloc.push_string(c);    
        ctx.push_stack(alloc.offset());
    };

    std::string cwd_env("__CWD=");
    cwd_env.append(cpp_getcwd());
    alloc.push_string(cwd_env.c_str());
    ctx.push_stack(alloc.offset());

    alloc.push_string("__PFX=//1");
    ctx.push_stack(alloc.offset());

    /* Argv */
    m_file_name.resize(Qnx::QPATH_MAX_T);
    if (!realpath(argv[0], m_file_name.data())) {
        throw std::system_error(errno, std::system_category());
    }
    m_file_name.resize(strlen(m_file_name.data()));

    ctx.push_stack(0);
    for (int i = argc - 1; i >= 0; i--) {
        alloc.push_string(argv[i]);
        ctx.push_stack(alloc.offset());
    }

    /* Argc */
    ctx.push_stack(argc);

    /* Entry points*/
    auto main_entry = m_load.entry_main.value();
    ctx.push_stack(main_entry.m_segment);
    ctx.push_stack(main_entry.m_offset);

    auto slib_entry = m_load.entry_slib.value();
    ctx.reg_cs() = slib_entry.m_segment;
    ctx.reg_eip() = slib_entry.m_offset;

    Log::print(Log::LOADER, "Real start: %x:%x\n", slib_entry.m_segment, slib_entry.m_offset);
    Log::print(Log::LOADER, "Program start: %x:%x\n", main_entry.m_segment, main_entry.m_offset);

    data_sd->update_descriptors();

    /* What we have allocated */
    ctx.reg_ebx() = data_seg->size();
    /* No, there is nothing free, allocate your own :) */
    ctx.reg_ecx() = 0;

    // ctx.dump(stdout);

    // breakpoints
    //ctx.write<uint8_t>(Context::CS, 0x61e , 0xCC);
}

ProcessFd* Process::fd_get(int id) {
    /* fields are default initialised */
    return &m_fds[id];
}

void Process::fd_release(int id) {
    m_fds.erase(id);
}
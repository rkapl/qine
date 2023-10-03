#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>

#include "emu.h"
#include "gen_msg/io.h"
#include "mem_ops.h"
#include "msg/meta.h"
#include "process.h"
#include "qnx/errno.h"
#include "qnx/magic.h"
#include "qnx/types.h"
#include "qnx/msg.h"
#include "msg/dump.h"
#include "gen_msg/proc.h"
#include "segment.h"
#include "segment_descriptor.h"
#include "types.h"
#include "context.h"
#include "msg.h"

Emu::Emu() {}

void Emu::init() {
    m_stack = Process::current()->allocate_segment();
    size_t stack_size = MemOps::PAGE_SIZE*8;
    size_t with_guard = MemOps::PAGE_SIZE + stack_size;
    m_stack->reserve(with_guard);
    m_stack->skip(MemOps::PAGE_SIZE);
    m_stack->grow(Access::READ_WRITE, stack_size);

    stack_t sas = {};
    sas.ss_size = MemOps::PAGE_SIZE*8;
    sas.ss_sp = reinterpret_cast<void*>(m_stack->location() + MemOps::PAGE_SIZE);
    sas.ss_flags = 0;
    if (sigaltstack(&sas, nullptr) != 0) {
        throw std::runtime_error(strerror(errno));
    }
}

void Emu::static_handler_segv(int sig, siginfo_t *info, void *uctx_void) {
    Process::current()->m_emu.handler_segv(sig, info, uctx_void);
}

void Emu::handler_segv(int sig, siginfo_t *info, void *uctx_void)
{
    ExtraContext ectx;
    ectx.from_cpu();
    auto ctx = Context(reinterpret_cast<ucontext_t*>(uctx_void), &ectx);
    auto eip = ctx.reg_eip();
    bool handled = false;

    if (ctx.reg_es() == Qnx::MAGIC_PTR_SELECTOR) {
        // hack: migrate to LDT
        ctx.reg_es() = Qnx::MAGIC_PTR_SELECTOR | 4;
        // printf("Migrating to LDT @ %x\n", ctx.reg(REG_EIP));
        handled = true;
    }

    if (ctx.read<uint8_t>(Context::CS, eip) == 0xCD && ctx.read<uint8_t>(Context::CS, eip + 1) == 0xF2) {
        dispatch_syscall(ctx);
        ctx.reg_eip() += 2;
        handled = true;
    }
    
    if (!handled) {
        ctx.dump(stdout);
        debug_hook();
    }
    ectx.to_cpu();
}

void Emu::dispatch_syscall(Context& ctx)
{
    uint8_t syscall = ctx.reg_al();
    switch (syscall) {
        case 0:
            syscall_sendmx(ctx);
            break;
        case 11:
            syscall_sendfdmx(ctx);
            break;
        default:
            printf("Unknown syscall %d\n", syscall);
            ctx.proc()->set_errno(Qnx::QENOSYS);
            ctx.reg_eax() = -1;
    }
}

void Emu::syscall_sendfdmx(Context &ctx)
{
    uint32_t ds = ctx.reg_ds();
    int fd = ctx.reg_edx();
    uint8_t send_parts = ctx.reg_ah();
    uint8_t recv_parts = ctx.reg_ch();
    GuestPtr send_data = ctx.reg_ebx();
    GuestPtr recv_data = ctx.reg_esi();

    Msg msg(Process::current(), send_parts, FarPointer(ds, send_data),  recv_parts, FarPointer(ds, recv_data));

    dispatch_msg(1, ctx, msg);
}

void Emu::syscall_sendmx(Context &ctx)
{
    uint32_t ds = ctx.reg_ds();
    Qnx::pid_t pid = ctx.reg_edx();
    uint8_t send_parts = ctx.reg_ah();
    uint8_t recv_parts = ctx.reg_ch();
    GuestPtr send_data = ctx.reg_ebx();
    GuestPtr recv_data = ctx.reg_esi();

    Msg msg(Process::current(), send_parts, FarPointer(ds, send_data),  recv_parts, FarPointer(ds, recv_data));
    // TODO: handle faults
    dispatch_msg(pid, ctx, msg);
}

void Emu::dispatch_msg(int pid, Context& ctx, Msg& msg)
{
    ctx.reg_eax() = 0;
    switch (pid) {
    case 1:
        msg_handle(msg);
        break;
    default:
        printf("Unknown message destination %d\n", pid);
        ctx.proc()->set_errno(Qnx::QESRCH);
        ctx.reg_eax() = -1;
    }
}

void Emu::msg_handle(Msg& msg) {
    Qnx::MsgHeader hdr;
    msg.read_type(&hdr);

#if 0
    uint8_t msg_class = hdr.type >> 8;
    switch (msg_class) {
        case 0: 
            QnxMsg::dump_message(stdout, QnxMsg::proc::list, msg);
            break;
        case 1: 
            QnxMsg::dump_message(stdout, QnxMsg::io::list, msg);
            break;
    }
#endif

    switch (hdr.type) {
        case QnxMsg::proc::segment_realloc::type:
            switch (hdr.subtype) {
                case QnxMsg::proc::segment_realloc::subtype:
                    proc_segment_realloc(msg);
                    break;
            }
            break;
        case QnxMsg::proc::terminate::type:
            proc_terminate(msg);
            break;

        case QnxMsg::io::write::type:
            io_write(msg);
            break;
        case QnxMsg::io::lseek::type:
            io_lseek(msg);
            break;
        default:
            printf("Unhandled message %x:%x\n", hdr.type, hdr.subtype);
            break;
    }
}

void Emu::proc_terminate(Msg &ctx)
{
    QnxMsg::proc::terminate msg;
    ctx.read_type(&msg);
    debug_hook();
    exit(msg.m_status);
}

void Emu::proc_segment_realloc(Msg &ctx)
{
    QnxMsg::proc::segment_realloc msg;
    ctx.read_type(&msg);
    // TODO: handle invalid segments
    auto sd  = Process::current()->descriptor_by_selector(msg.m_sel);
    auto seg = sd->segment();
    GuestPtr base = seg->paged_size();

    QnxMsg::proc::segment_realloc_reply reply;
    memset(&reply, 0, sizeof(reply));
    if (seg->is_shared()) {
        reply.m_status = Qnx::QEBUSY;
    } else {
        if (seg->size() < msg.m_nbytes) {
            seg->grow(Access::READ_WRITE, MemOps::align_page_up(msg.m_nbytes - seg->size()));
            // TODO: this must be done better, the segment must be aware of its descriptors,
            // at least code and data 
            sd->update_descriptors();
        }
        reply.m_status = Qnx::QEOK;
        reply.m_sel = msg.m_sel;
        reply.m_nbytes = seg->size();
        reply.m_addr = reinterpret_cast<uint32_t>(seg->location()); 
        //printf("New segment size: %x\n", seg->size());
    }
    ctx.write_type(0, &reply);
}

void Emu::io_lseek(Msg &ctx) {
    QnxMsg::io::lseek msg;
    ctx.read_type(&msg);

    // STUB

    QnxMsg::io::lseek_reply reply;
    reply.m_status = Qnx::QEOK;
    reply.m_zero = 0;
    reply.m_offset = 0;
    ctx.write_type(0, &reply);
}

void Emu::io_write(Msg &ctx) {
    QnxMsg::io::write msg;
    ctx.read_type(&msg);

    std::vector<struct iovec> iov;
    ctx.read_iovec(sizeof(msg), msg.m_nbytes, iov);

    QnxMsg::io::write_reply reply;
    int r = writev(msg.m_fd, iov.data(), iov.size());
    reply.m_zero = 0;
    if (r < 0) {
        reply.m_status = errno;
        reply.m_nbytes = 0;
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_nbytes = r;
    }
    ctx.write_type(0, &reply);
}

void Emu::debug_hook() {

}

void Emu::static_handler_user(int sig, siginfo_t *info, void *uctx)
{
    ExtraContext ectx;
    ectx.from_cpu();
    auto sig_ctx = reinterpret_cast<ucontext_t*>(uctx);
    Context ctx(sig_ctx, &ectx);
    auto proc = Process::current();

    #define SYNC(x) ctx.reg_##x() = proc->m_startup_context.reg_##x()
    SYNC(eip);
    SYNC(cs);
    SYNC(ss);
    SYNC(esp);
    SYNC(ds);
    SYNC(es);
    SYNC(fs);
    SYNC(gs);

    SYNC(eax);
    SYNC(ebx);
    SYNC(ecx);
    SYNC(edx);
    SYNC(eax);
    SYNC(esi);
    SYNC(edi);
    #undef SYNC

    ectx.to_cpu();
    printf("Entering emulation\n");
}

void Emu::enter_emu() {
    struct sigaction sa = {};
    sa.sa_sigaction = Emu::static_handler_segv;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, nullptr) == -1){
        throw std::runtime_error(strerror(errno));
    }

    /* We abuse the signal mechanism for a context switch*/
    sa.sa_sigaction = Emu::static_handler_user;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, nullptr)) {
        throw std::runtime_error(strerror(errno));
    }
    kill(getpid(), SIGUSR1);
}

Emu::~Emu() {
    if (!m_stack)
        return;

    signal(SIGUSR1, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);

    stack_t sas;
    sas.ss_size = 0;
    sas.ss_sp = 0;
    sas.ss_flags = SS_DISABLE;
    sigaltstack(&sas, nullptr);
}
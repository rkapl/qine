#include <stdexcept>
#include <vector>
#include <errno.h>
#include <string.h>

#include "emu.h"
#include "gen_msg/io.h"
#include "log.h"
#include "mem_ops.h"
#include "msg/meta.h"
#include "msg_handler.h"
#include "qnx/errno.h"
#include "qnx/magic.h"
#include "qnx/types.h"
#include "qnx/msg.h"
#include "msg/dump.h"
#include "process.h"
#include "segment.h"
#include "segment_descriptor.h"
#include "types.h"
#include "context.h"
#include "msg.h"

#include <gen_msg/proc.h>
#include <gen_msg/io.h>

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

qine_no_tls void Emu::handler_segv(int sig, siginfo_t *info, void *uctx_void)
{
    m_tls_fixup.restore();

    // now we have normal C environment
    ExtraContext ectx;
    ectx.from_cpu();
    auto ctx = Context(reinterpret_cast<ucontext_t*>(uctx_void), &ectx);
    if ((ctx.reg_cs() & SegmentDescriptor::SEL_LDT) == 0) {
        debug_hook();
        fprintf(stderr, "Sigsegv in host code\n");
        exit(1);
    }
    auto eip = ctx.reg_eip();
    bool handled = false;

    if (ctx.reg_es() == Qnx::MAGIC_PTR_SELECTOR) {
        // hack: migrate to LDT or patch the code
        ctx.reg_es() = Qnx::MAGIC_PTR_SELECTOR | 4;
        // printf("Migrating to LDT @ %x\n", ctx.reg_eip());
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
    auto proc = Process::current();
    uint32_t ds = ctx.reg_ds();
    int fd = ctx.reg_edx();
    uint8_t send_parts = ctx.reg_ah();
    uint8_t recv_parts = ctx.reg_ch();
    GuestPtr send_data = ctx.reg_ebx();
    GuestPtr recv_data = ctx.reg_esi();

    Msg msg(proc, send_parts, FarPointer(ds, send_data),  recv_parts, FarPointer(ds, recv_data));
    MsgInfo info;
    info.m_ctx = &ctx;
    info.m_msg = &msg;
    info.m_via_fd = true;
    info.m_fd = fd;

    proc->handle_msg(info);
}

void Emu::syscall_sendmx(Context &ctx)
{
    auto proc = Process::current();
    
    uint32_t ds = ctx.reg_ds();
    Qnx::pid_t pid = ctx.reg_edx();
    uint8_t send_parts = ctx.reg_ah();
    uint8_t recv_parts = ctx.reg_ch();
    GuestPtr send_data = ctx.reg_ebx();
    GuestPtr recv_data = ctx.reg_esi();

    Msg msg(Process::current(), send_parts, FarPointer(ds, send_data),  recv_parts, FarPointer(ds, recv_data));
    // TODO: handle faults etc.
    MsgInfo info;
    info.m_ctx = &ctx;
    info.m_msg = &msg;
    info.m_via_fd = false;
    info.m_pid = pid;

    proc->handle_msg(info);
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
    Log::print(Log::MAIN, "Entering emulation\n");
}

void Emu::enter_emu() {
    m_tls_fixup.save();

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

#define ERRNO_MAP \
    MAP(0, Qnx::QEOK) \
    MAP(EPERM, Qnx::QEPERM) \
    MAP(ENOENT, Qnx::QENOENT) \
    MAP(ESRCH, Qnx::QESRCH) \
    MAP(EINTR, Qnx::QEINTR) \
    MAP(EIO, Qnx::QEIO) \
    MAP(ENXIO, Qnx::QENXIO) \
    MAP(ENOEXEC, Qnx::QENOEXEC) \
    MAP(EBADF, Qnx::QEBADF) \


Qnx::errno_t Emu::map_errno(int v) {
    #define MAP(l, q) case l: return q;
    switch(v) {
        ERRNO_MAP
        default:
            return Qnx::QEIO;
    }
    #undef MAP
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
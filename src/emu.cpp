#include <bits/types/sigset_t.h>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <errno.h>
#include <string.h>

#include "compiler.h"
#include "emu.h"
#include "gen_msg/io.h"
#include "log.h"
#include "mem_ops.h"
#include "msg/meta.h"
#include "msg_handler.h"
#include "qnx/errno.h"
#include "qnx/magic.h"
#include "qnx/procenv.h"
#include "qnx/signal.h"
#include "qnx/types.h"
#include "qnx/msg.h"
#include "msg/dump.h"
#include "process.h"
#include "segment.h"
#include "segment_descriptor.h"
#include "types.h"
#include "guest_context.h"
#include "msg.h"

#include <gen_msg/proc.h>
#include <gen_msg/io.h>

Emu::Emu() {}

void Emu::init() {
    m_emulation_stack = Process::current()->allocate_segment();
    size_t stack_size = MemOps::PAGE_SIZE*8;
    size_t with_guard = MemOps::PAGE_SIZE + stack_size;
    m_emulation_stack->reserve(with_guard);
    m_emulation_stack->skip(MemOps::PAGE_SIZE);
    m_emulation_stack->grow(Access::READ_WRITE, stack_size);

    stack_t sas = {};
    sas.ss_size = MemOps::PAGE_SIZE*8;
    sas.ss_sp = reinterpret_cast<void*>(m_emulation_stack->location() + MemOps::PAGE_SIZE);
    sas.ss_flags = 0;
    if (sigaltstack(&sas, nullptr) != 0) {
        throw std::runtime_error(strerror(errno));
    }
}

qine_no_tls void Emu::static_handler_segv(int sig, siginfo_t *info, void *uctx_void) {
    Process::current()->m_emu.handler_segv(sig, info, uctx_void);
}

qine_no_tls void Emu::handler_segv(int sig, siginfo_t *info, void *uctx_void)
{
    ExtraContext ectx;
    ectx.from_cpu();
    m_tls_fixup.restore();

    // now we have normal C environment
    debug_hook_sig_enter();

    // unblock the signal, for better debugging and to not let someone inherit it
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGSEGV);
    sigprocmask(SIG_UNBLOCK, &ss, nullptr);

    auto ctx = GuestContext(reinterpret_cast<ucontext_t*>(uctx_void), &ectx);
    if ((ctx.reg_cs() & SegmentDescriptor::SEL_LDT) == 0) {
        debug_hook_problem();
        fprintf(stderr, "Sigsegv in host code\n");
        exit(1);
    }
    auto eip = ctx.reg_eip();
    bool handled = false;

    if (ctx.reg_es() == Qnx::MAGIC_PTR_SELECTOR) {
        // hack: migrate to LDT instead of patching the code
        ctx.reg_es() = Qnx::MAGIC_PTR_SELECTOR | 4;
        //printf("Migrating to LDT @ %x\n", ctx.reg_eip());
        handled = true;
    }

    if (ctx.read<uint8_t>(GuestContext::CS, eip) == 0xCD && ctx.read<uint8_t>(GuestContext::CS, eip + 1) == 0xF2) {
        dispatch_syscall(ctx);
        ctx.reg_eip() += 2;
        handled = true;
    }
    
    if (!handled) {
        signal_raise(Qnx::QSIGSEGV);
    }

    signal_tail(ctx);
}

qine_no_tls void Emu::static_handler_generic(int sig, siginfo_t *info, void *uctx) {
    Process::current()->m_emu.handler_generic(sig, info, uctx);;
}

qine_no_tls void Emu::handler_generic(int sig, siginfo_t *info, void *uctx_void) {
    ExtraContext ectx;
    ectx.from_cpu();
    m_tls_fixup.restore();
    
    // now we have normal C environment
    auto ctx = GuestContext(reinterpret_cast<ucontext_t*>(uctx_void), &ectx);


    int qnx_sig = map_sig_host_to_qnx(sig);
    if (qnx_sig == -1) {
        // not async safe, but we are screwed anyway
        throw GuestStateException("Received signal, it is registered, but not QNX signal.");
    }

    m_sigpend |= (1u << qnx_sig);

    signal_tail(ctx);
}

/* 
 * This functions is called on exit from a host signal and is responsible 
 * for setting up QNX signal state if there is a pending signal.
 */
qine_no_tls void Emu::signal_tail(GuestContext& ctx) {
    auto activesig = (~m_sigmask) & m_sigpend;

    if ((ctx.reg_cs() & SegmentDescriptor::SEL_LDT) == 0) {
        /* We are returning to host code -- do not do anyting special */
        return;
    }

    if (activesig == 0) {
        ctx.m_ectx->to_cpu();
        return;
    }

    /* Find and pop the signal */
    
    int qnx_sig;
    for (qnx_sig = 0; qnx_sig < Qnx::QSIG_COUNT; qnx_sig++) {
        if (activesig & (1u << qnx_sig))
            break;
    }
    m_sigpend &= ~(1u << qnx_sig);
    
    /* Find it */
    auto proc = ctx.proc();
    if (!proc->m_sigtab) {
        ctx.dump(stdout);
        debug_hook_problem();
        throw GuestStateException("Received signal, but no sigtab registered by guest.");
    }

    auto act = &proc->m_sigtab->actions[qnx_sig];
    if (act->handler_fn == Qnx::QSIG_DFL || act->handler_fn == Qnx::QSIG_ERR || act->handler_fn == Qnx::QSIG_HOLD) {
        ctx.dump(stdout);
        debug_hook_problem();
        throw GuestStateException("Received signal, but handler not registered");
    }

    uint32_t mask = act->mask | (1u << qnx_sig);
    m_sigmask |= mask;
    
    ctx.save_context();

    // TODO: does QNX have redline?
    // TODO: does QNX have siginfo?

    // our stuff
    ctx.push_stack(mask);

    // arguments to sigstub
    ctx.push_stack(0); // unknown
    ctx.push_stack(qnx_sig);
    ctx.push_stack(act->handler_fn);

    ctx.reg_cs() = proc->m_load.entry_main->m_segment;
    ctx.reg_eip() = proc->m_sigtab->sigstub;
    Log::print(Log::SIG, "raised signal %d\n", qnx_sig);
    ctx.m_ectx->to_cpu();
}

void Emu::syscall_sigreturn(GuestContext &ctx)
{
    uint32_t mask = ctx.pop_stack();
    ctx.restore_context();

    m_sigmask &= ~mask;
}


void Emu::dispatch_syscall(GuestContext& ctx)
{
    uint8_t syscall = ctx.reg_al();
    switch (syscall) {
        case 0:
            syscall_sendmx(ctx);
            break;
        case 1:
            syscall_receivmx(ctx);
            break;
        case 7:
            syscall_sigreturn(ctx);
            break;
        case 11:
            syscall_sendfdmx(ctx);
            break;
        case 14:
            syscall_kill(ctx);
            break;
        default:
            printf("Unknown syscall %d\n", syscall);
            ctx.proc()->set_errno(Qnx::QENOSYS);
            ctx.reg_eax() = -1;
    }
}

void Emu::syscall_sendfdmx(GuestContext &ctx)
{
    auto proc = Process::current();
    uint32_t ds = ctx.reg_ds();
    int fd = ctx.reg_edx();
    uint8_t send_parts = ctx.reg_ah();
    uint8_t recv_parts = ctx.reg_ch();
    GuestPtr send_data = ctx.reg_ebx();
    GuestPtr recv_data = ctx.reg_esi();

    Msg msg(proc, send_parts, FarPointer(ds, send_data),  recv_parts, FarPointer(ds, recv_data));
    MsgContext info;
    info.m_ctx = &ctx;
    info.m_msg = &msg;
    info.m_proc = proc;
    info.m_via_fd = true;
    info.m_fd = fd;

    proc->handle_msg(info);
}

void Emu::syscall_receivmx(GuestContext &ctx)
{
    // we do not support messages yet, but slib:pause uses it as e.g. "pause" (=wait for signal)
    Qnx::pid_t pid = ctx.reg_edx();
    uint8_t rcv_parts = ctx.reg_ah();
    GuestPtr rmsg = ctx.reg_ebx();

    if (pid == QnxPid::PID_PROC || pid == 0) {
        pause();
        ctx.reg_eax() = Qnx::QEINTR;
    } else {
        ctx.reg_eax() = Qnx::QESRCH;
    }
}

void Emu::syscall_sendmx(GuestContext &ctx)
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
    MsgContext info;
    info.m_ctx = &ctx;
    info.m_msg = &msg;
    info.m_proc = proc;
    info.m_via_fd = false;
    info.m_pid = pid;

    proc->handle_msg(info);
}

void Emu::syscall_kill(GuestContext &ctx)
{
    int pid = ctx.reg_edx();
    int qnx_signo = ctx.reg_ebx();
    int host_signo = map_sig_qnx_to_host(qnx_signo);
    if (host_signo == -1) {
        ctx.reg_eax() = Qnx::QEINVAL;
        return;
    }
    if (pid != Process::current()->pid()) {
        ctx.reg_eax() = Qnx::QESRCH;
        return;
    }
    signal_raise(qnx_signo);
    ctx.reg_eax() = Qnx::QEOK;
}

void Emu::signal_raise(int qnx_sig)
{
    m_sigpend |= (1u << qnx_sig);
}

void Emu::signal_mask(uint32_t change_mask, uint32_t bits)
{
    m_sigmask = (m_sigmask & ~change_mask) | bits;
}

uint32_t Emu::signal_getmask() {
    return m_sigmask;
}

void Emu::debug_hook_problem() {
}

void Emu::debug_hook_sig_enter() {
}

qine_no_tls void Emu::static_handler_user(int sig, siginfo_t *info, void *uctx)
{
    ExtraContext ectx;
    ectx.from_cpu();
    auto sig_ctx = reinterpret_cast<ucontext_t*>(uctx);
    GuestContext ctx(sig_ctx, &ectx);
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

    Log::print(Log::LOADER, "Entering emulation\n");

    ectx.to_cpu();
}

void Emu::enter_emu() {
    m_tls_fixup.save();

    struct sigaction sa = {0};
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
    if (sigaction(SIGUSR1, &sa, nullptr) == -1) {
        throw std::runtime_error(strerror(errno));
    }
    raise(SIGUSR1);
}

int Emu::signal_sigact(int qnx_sig, uint32_t handler, uint32_t mask)
{
    auto sigtab = Process::current()->m_sigtab;
    if (!sigtab) {
        throw GuestStateException("Sigtab not registered");
    }

    int host_sig = map_sig_qnx_to_host(qnx_sig);
    if (host_sig == -1) {
        return Qnx::QEINVAL;
    }

    if (host_sig != SIGSEGV) {
        struct sigaction sa = {0};
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigfillset(&sa.sa_mask);
        
        if (handler == Qnx::QSIG_DFL) {
            sa.sa_handler = SIG_DFL;
        } else if (handler == Qnx::QSIG_IGN) {
            sa.sa_handler = SIG_IGN;
        } else if (handler == Qnx::QSIG_ERR) {
            sa.sa_handler = SIG_ERR;
        } else if (handler == Qnx::QSIG_HOLD) {
            sa.sa_handler = SIG_HOLD;
        } else {
            sa.sa_sigaction = static_handler_generic;
        }
        
        int r = sigaction(host_sig, &sa, NULL);
        if (r != 0) {
            return map_errno(r);
        }
    }

    Log::print(Log::SIG, "Registered signal %d, handler %x\n", qnx_sig, handler);   
    auto si = &sigtab->actions[qnx_sig];
    si->flags = 0;
    si->handler_fn = handler;
    si->mask = mask;
    
    return Qnx::QEOK;
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
    MAP(ECHILD, Qnx::QECHILD) \
    MAP(EAGAIN, Qnx::QEAGAIN) \
    MAP(ENOMEM, Qnx::QENOMEM) \
    MAP(EACCES, Qnx::QEACCES) \
    MAP(EFAULT, Qnx::QEFAULT) \
    MAP(ENOTBLK, Qnx::QENOTBLK) \
    MAP(EBUSY, Qnx::QEBUSY) \
    MAP(EEXIST, Qnx::QEEXIST) \
    MAP(EXDEV, Qnx::QEXDEV) \
    MAP(ENODEV, Qnx::QENODEV) \
    MAP(ENOTDIR, Qnx::QENOTDIR) \
    MAP(EISDIR, Qnx::QEISDIR) \
    MAP(EINVAL, Qnx::QEINVAL) \
    MAP(ENFILE, Qnx::QENFILE) \
    MAP(EMFILE, Qnx::QEMFILE) \
    MAP(ENOTTY, Qnx::QENOTTY) \
    MAP(ETXTBSY, Qnx::QETXTBSY) \
    MAP(EFBIG, Qnx::QEFBIG) \
    MAP(ENOSPC, Qnx::QENOSPC) \
    MAP(ESPIPE, Qnx::QESPIPE) \
    MAP(EROFS, Qnx::QEROFS) \
    MAP(EMLINK, Qnx::QEMLINK) \
    MAP(EPIPE, Qnx::QEPIPE) \
    MAP(EDOM, Qnx::QEDOM) \
    MAP(ERANGE, Qnx::QERANGE) \
    MAP(ENOMSG, Qnx::QENOMSG) \
    MAP(EIDRM, Qnx::QEIDRM) \
    MAP(ECHRNG, Qnx::QECHRNG) \
    MAP(EL2NSYNC, Qnx::QEL2NSYNC) \
    MAP(EL3HLT, Qnx::QEL3HLT) \
    MAP(EL3RST, Qnx::QEL3RST) \
    MAP(ELNRNG, Qnx::QELNRNG) \
    MAP(EUNATCH, Qnx::QEUNATCH) \
    MAP(ENOCSI, Qnx::QENOCSI) \
    MAP(EL2HLT, Qnx::QEL2HLT) \
    MAP(EDEADLK, Qnx::QEDEADLK) \
    MAP(ENOLCK, Qnx::QENOLCK) \
    MAP(ENOTSUP, Qnx::QENOTSUP) \
    MAP(ENAMETOOLONG, Qnx::QENAMETOOLONG) \
    MAP(ELIBACC, Qnx::QELIBACC) \
    MAP(ELIBBAD, Qnx::QELIBBAD) \
    MAP(ELIBSCN, Qnx::QELIBSCN) \
    MAP(ELIBMAX, Qnx::QELIBMAX) \
    MAP(ELIBEXEC, Qnx::QELIBEXEC) \
    MAP(ENOSYS, Qnx::QENOSYS) \
    MAP(ELOOP, Qnx::QELOOP) \
    MAP(ENOTEMPTY, Qnx::QENOTEMPTY) \
    MAP(ESTALE, Qnx::QESTALE) \
    MAP(EINPROGRESS, Qnx::QEINPROGRESS) \
    MAP(EALREADY, Qnx::QEALREADY) \
    MAP(ENOTSOCK, Qnx::QENOTSOCK) \
    MAP(EDESTADDRREQ, Qnx::QEDESTADDRREQ) \
    MAP(EMSGSIZE, Qnx::QEMSGSIZE) \
    MAP(EPROTOTYPE, Qnx::QEPROTOTYPE) \
    MAP(ENOPROTOOPT, Qnx::QENOPROTOOPT) \
    MAP(EPROTONOSUPPORT, Qnx::QEPROTONOSUPPORT) \
    MAP(ESOCKTNOSUPPORT, Qnx::QESOCKTNOSUPPORT) \
    MAP(EPFNOSUPPORT, Qnx::QEPFNOSUPPORT) \
    MAP(EAFNOSUPPORT, Qnx::QEAFNOSUPPORT) \
    MAP(EADDRINUSE, Qnx::QEADDRINUSE) \
    MAP(EADDRNOTAVAIL, Qnx::QEADDRNOTAVAIL) \
    MAP(ENETDOWN, Qnx::QENETDOWN) \
    MAP(ENETUNREACH, Qnx::QENETUNREACH) \
    MAP(ENETRESET, Qnx::QENETRESET) \
    MAP(ECONNABORTED, Qnx::QECONNABORTED) \
    MAP(ECONNRESET, Qnx::QECONNRESET) \
    MAP(ENOBUFS, Qnx::QENOBUFS) \
    MAP(EISCONN, Qnx::QEISCONN) \
    MAP(ENOTCONN, Qnx::QENOTCONN) \
    MAP(ESHUTDOWN, Qnx::QESHUTDOWN) \
    MAP(ETOOMANYREFS, Qnx::QETOOMANYREFS) \
    MAP(ETIMEDOUT, Qnx::QETIMEDOUT) \
    MAP(ECONNREFUSED, Qnx::QECONNREFUSED) \
    MAP(EHOSTDOWN, Qnx::QEHOSTDOWN) \
    MAP(EHOSTUNREACH, Qnx::QEHOSTUNREACH) \
    MAP(EREMOTE, Qnx::QEREMOTE) \

Qnx::errno_t Emu::map_errno(int v) {
    #define MAP(l, q) case l: return q;
    switch(v) {
        ERRNO_MAP
        default:
            return Qnx::QEIO;
    }
    #undef MAP
}

#define SIG_MAP \
    MAP(SIGHUP, Qnx::QSIGHUP) \
    MAP(SIGINT, Qnx::QSIGINT) \
    MAP(SIGQUIT, Qnx::QSIGQUIT) \
    MAP(SIGILL, Qnx::QSIGILL) \
    MAP(SIGTRAP, Qnx::QSIGTRAP) \
    MAP(SIGIOT, Qnx::QSIGIOT) \
    MAP(SIGFPE, Qnx::QSIGFPE) \
    MAP(SIGKILL, Qnx::QSIGKILL) \
    MAP(SIGBUS, Qnx::QSIGBUS) \
    MAP(SIGSEGV, Qnx::QSIGSEGV) \
    MAP(SIGSYS, Qnx::QSIGSYS) \
    MAP(SIGPIPE, Qnx::QSIGPIPE) \
    MAP(SIGALRM, Qnx::QSIGALRM) \
    MAP(SIGTERM, Qnx::QSIGTERM) \
    MAP(SIGUSR1, Qnx::QSIGUSR1) \
    MAP(SIGUSR2, Qnx::QSIGUSR2) \
    MAP(SIGCHLD, Qnx::QSIGCHLD) \
    MAP(SIGPWR, Qnx::QSIGPWR) \
    MAP(SIGWINCH, Qnx::QSIGWINCH) \
    MAP(SIGURG, Qnx::QSIGURG) \
    MAP(SIGIO, Qnx::QSIGIO) \
    MAP(SIGSTOP, Qnx::QSIGSTOP) \
    MAP(SIGCONT, Qnx::QSIGCONT) \
    MAP(SIGTTIN, Qnx::QSIGTTIN) \
    MAP(SIGTTOU, Qnx::QSIGTTOU)

// handle sigtstp, dev and emt separately

int Emu::map_sig_host_to_qnx(int sig) {
    #define MAP(h, q) case h: return q;
    switch(sig) {
        SIG_MAP
        default:
            return -1;
    }
    #undef MAP
}

int Emu::map_sig_qnx_to_host(int sig) {
    #define MAP(h, q) case q: return h;
    switch(sig) {
        SIG_MAP
        default:
            return -1;
    }
    #undef MAP
}

sigset_t Emu::map_sigmask_qnx_to_host(uint32_t mask) {
    sigset_t acc;
    sigemptyset(&acc);
    for (int sig = 0; sig < 32; sig++) {
        int host_sig = map_sig_qnx_to_host(sig);
        if (host_sig)
            continue;
        if (mask & (1u << sig))
            sigaddset(&acc, host_sig);
    }
    return acc;
}

Emu::~Emu() {
    if (!m_emulation_stack)
        return;

    signal(SIGUSR1, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);

    stack_t sas;
    sas.ss_size = 0;
    sas.ss_sp = 0;
    sas.ss_flags = SS_DISABLE;
    sigaltstack(&sas, nullptr);
}
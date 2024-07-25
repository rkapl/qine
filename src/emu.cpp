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
#include "qnx/sem.h"
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
    m_emulation_stack->skip_paged(MemOps::PAGE_SIZE);
    m_emulation_stack->grow_paged(Access::READ_WRITE, stack_size);

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

void Emu::handle_guest_segv(GuestContext &ctx, siginfo_t *info) {
    auto act = qnx_sigtab(ctx.proc(), Qnx::QSIGSEGV, false);
    // since we need to inspect all sigsegvs, we now need to emulate the behavior
    if (!act || is_special_sighandler(act->handler_fn)) {
        // sigsegv cannot be ignored/blocked
        fprintf(stderr, "Sigsegv in guest code, si_code=%x\n", info->si_code);
        ctx.dump(stderr);
        debug_hook_problem();
        abort();
    } else {
        Log::print(Log::SIG, "Propagating SIGSEGV to host, handler will be %x\n", act->handler_fn);
        signal_raise(Qnx::QSIGSEGV);
    }
}

qine_no_tls void Emu::handler_segv(int sig, siginfo_t *info, void *uctx_void)
{
    ExtraContext ectx;
    ectx.from_cpu();
    m_tls_fixup.restore();

    // now we have normal C environment
    debug_hook_sig_enter();

    auto ctx = GuestContext(reinterpret_cast<ucontext_t*>(uctx_void), &ectx);
    if (info->si_code <= 0) {
        // this is not a real sigsegv, someone sent it (e.g. bash likes to send signals to itself)
        handle_guest_segv(ctx, info);
        signal_tail(ctx);
        return;
    }

    // If we are in our code, it is our problem
    if ((ctx.reg_cs() & SegmentDescriptor::SEL_LDT) == 0) {
        debug_hook_problem();
        fprintf(stderr, "Sigsegv in host code, %p, si_code=%x\n", reinterpret_cast<void*>(ctx.reg_eip()), info->si_code);
        abort();
    }

    // unblock the signal, for better debugging, to avoid someone inheriting the mask if exec'd
    // and to sent sigsegvs immediately
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGSEGV);
    sigprocmask(SIG_UNBLOCK, &ss, nullptr);

    bool handled = false;

    if (ctx.reg_es() == Qnx::MAGIC_PTR_SELECTOR) {
        // hack: migrate to LDT instead of patching the code
        ctx.reg_es() = Qnx::MAGIC_PTR_SELECTOR | 4;
        //printf("Migrating to LDT @ %x\n", ctx.reg_eip());
        handled = true;
    }

    if (ctx.reg_ds() == Qnx::MAGIC_PTR_SELECTOR) {
        ctx.reg_ds() = Qnx::MAGIC_PTR_SELECTOR | 4;
        handled = true;
    }

    int insn_len;
    if (matches_syscall(ctx, 0xF2, &insn_len)) {
        ctx.reg_eip() += insn_len;
        // note: syscall includes sysreturn which may change eip
        dispatch_syscall(ctx);
        handled = true;
    } else if (matches_syscall(ctx, 0xF1, &insn_len)) {
        ctx.reg_eip() += insn_len;
        dispatch_syscall_sem(ctx);
        handled = true;
    } else if (matches_syscall(ctx, 0xF0, &insn_len)) {
        ctx.reg_eip() += insn_len;
        dispatch_syscall16(ctx);
        handled = true;
    }
    
    
    if (!handled) {
        //ctx.dump(stderr, 64);
        //Emu::debug_hook_problem();
        handle_guest_segv(ctx, info);
    }

    signal_tail(ctx);
}

bool Emu::matches_syscall(GuestContext &ctx, int int_nr, int* insn_len) {
    *insn_len = 0;
    auto eip = ctx.reg_eip();
    auto b1 = ctx.read<uint8_t>(GuestContext::CS, eip);
    // prefix
    if (b1 == 0x66) {
        eip++;
        (*insn_len) ++;
        b1 = ctx.read<uint8_t>(GuestContext::CS, eip);
    }

    (*insn_len) += 2;
    return b1 == 0xCD && ctx.read<uint8_t>(GuestContext::CS, eip + 1) == int_nr;
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


    int qnx_sig = QnxSigset::map_sig_host_to_qnx(sig);
    if (qnx_sig == -1) {
        // not async safe, but we are screwed anyway
        throw GuestStateException("Received signal, it is registered, but not QNX signal.");
    }

    Log::print(Log::SIG, "Received signal %d\n", qnx_sig);;
    m_sigpend.set_qnx_sig(qnx_sig);

    signal_tail(ctx);
}

qine_no_tls void Emu::sync_host_sigmask(GuestContext &ctx) {
    // Synchronize the host sigmask state with emulated when we exit the signal
    // This is needed, because the signal mask and the list of pending signals
    // affects some calls, like TC SIGTTOU and tcsetpgrp
    ctx.saved_sigmask() = m_sigmask.map_to_host_sigset();
    sigdelset(&ctx.saved_sigmask(), SIGSEGV);
    //fprintf(stderr, "setting host sigmask %lx\n", ctx.saved_sigmask().__val[0]);
}

bool Emu::is_special_sighandler(uint32_t qnx_handler) {
    return 
        qnx_handler == Qnx::QSIG_DFL
        || qnx_handler == Qnx::QSIG_HOLD
        || qnx_handler == Qnx::QSIG_ERR
        || qnx_handler == Qnx::QSIG_IGN;
}

/* 
 * This functions is called on exit from a host signal and is responsible 
 * for setting up QNX signal state if there is a pending signal.
 */
qine_no_tls void Emu::signal_tail(GuestContext& ctx) {
    auto proc = ctx.proc();
    QnxSigset activesig = m_sigpend;
    activesig.modify(m_sigmask, QnxSigset::empty());

    if ((ctx.reg_cs() & SegmentDescriptor::SEL_LDT) == 0) {
        /* We are returning to host code -- do not do anyting special */
        return;
    }

    if (activesig.is_empty()) {
        // return to QNX without activating any signal
        sync_host_sigmask(ctx);
        ctx.m_ectx->to_cpu();
        return;
    }

    /* Signal emulation: Find and pop the signal */
    int qnx_sig = activesig.find_first();
    m_sigpend.clear_qnx_sig(qnx_sig);
    
    auto act = qnx_sigtab(proc, qnx_sig);
    if (is_special_sighandler(act->handler_fn)) {
        fprintf(stderr, "QNX signal %d has handler %x\n", qnx_sig, act->handler_fn);
        ctx.dump(stdout);
        debug_hook_problem();
        throw GuestStateException("Received signal, but handler not registered -- should not happen");
    }

    uint32_t save_sigmask = m_sigmask.m_value;
    m_sigmask.m_value |= act->mask;
    m_sigmask.set_qnx_sig(qnx_sig);

    sync_host_sigmask(ctx);

    // TODO: does QNX have redline?
    // TODO: does QNX have siginfo?
    uint32_t old_esp = ctx.reg_esp();
    ctx.reg_esp() -= REDLINE;
    
    ctx.save_context();

    // our stuff
    ctx.push_stack(save_sigmask);

    // arguments to sigstub
    ctx.push_stack(0); // unknown
    ctx.push_stack(qnx_sig);
    ctx.push_stack(act->handler_fn);

    ctx.reg_cs() = proc->m_load_exec.cs;
    ctx.reg_eip() = proc->m_sigtab->sigstub;
    Log::print(Log::SIG, "Emulating QNX signal %d, handler %x:%x, via stub %x, @esp %x, new mask %x, pending %x\n", 
        qnx_sig,
        proc->m_load_exec.cs, act->handler_fn, proc->m_sigtab->sigstub, 
        old_esp,
        m_sigmask.m_value, m_sigpend.m_value);
    ctx.m_ectx->to_cpu();
}

void Emu::syscall_sigreturn(GuestContext &ctx)
{
    m_sigmask = ctx.pop_stack();
    ctx.restore_context();

    ctx.reg_esp() += REDLINE;

    Log::print(Log::SIG, "Return from emulated signal, new mask %x, @esp %x, to %x:%x\n", 
        m_sigmask.m_value, ctx.reg_esp(),
        ctx.reg_cs(), ctx.reg_eip()
    );
}

void Emu::syscall_priority(GuestContext &ctx) {
    // ignore
    ctx.set_syscall_ok();
}

void Emu::dispatch_syscall(GuestContext& ctx)
{
    try {
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
            case 8:
                syscall_priority(ctx);;
                break;
            case 10:
                syscall_yield(ctx);
                break;
            case 11:
                syscall_sendfdmx(ctx);
                break;
            case 14:
                syscall_kill(ctx);
                break;
            default:
                printf("Unknown syscall %d\n", syscall);
                ctx.set_syscall_error(Qnx::QENOSYS);
        }
    } catch (const SegmentationFault& e) {
        Log::print(Log::UNHANDLED, "Segfault during message handling: %s\n", e.what());
        ctx.set_syscall_error(Qnx::QEFAULT);
    }
}

void Emu::dispatch_syscall_sem(GuestContext &ctx) {
    auto sem_addr = ctx.clip16(ctx.reg_eax());
    auto syscall = ctx.clip16(ctx.reg_ebx());
    auto sem = reinterpret_cast<Qnx::sem_t*>(
        ctx.proc()->translate_segmented(FarPointer(ctx.reg_ds(), sem_addr), sizeof(Qnx::sem_t), RwOp::WRITE)
    );

    // ASSUME: single-threaded
    if (syscall == 0) {
        // post
        sem->value++;
        ctx.set_syscall_ok();
    } else if (syscall == 1) {
        // wait
        if (sem->value == 0) {
            ctx.set_syscall_error(Qnx::QEDEADLK);
        } else {
            sem->value --;
            ctx.set_syscall_ok();
        }
    } else if (syscall == 2) {
        // try_again
        if (sem->value == 0) {
            ctx.set_syscall_error(Qnx::QEAGAIN);
        } else {
            ctx.set_syscall_ok();
            sem->value--;
        }
    } else {
        Log::print(Log::UNHANDLED, "Unknown semaphore syscall: %d\n", syscall);
        ctx.set_syscall_error(Qnx::QENOSYS);
    }
}

void Emu::syscall_yield(GuestContext &ctx) {
    sched_yield();
    ctx.set_syscall_ok();
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

    Msg msg(proc, send_parts, FarPointer(ds, send_data),  recv_parts, FarPointer(ds, recv_data), B32);
    MsgContext info;
    info.m_ctx = &ctx;
    info.m_msg = &msg;
    info.m_proc = proc;
    info.m_via_fd = true;
    info.m_fd = fd;

    proc->handle_msg(info);
    ctx.set_syscall_ok();
}

void Emu::syscall_receivmx(GuestContext &ctx)
{
    // we do not support messages yet, but slib:pause uses it as e.g. "pause" (=wait for signal)
    Qnx::pid_t pid = ctx.reg_edx();
    uint8_t rcv_parts = ctx.reg_ah();
    GuestPtr rmsg = ctx.reg_ebx();

    if (pid == ctx.proc()->pid() || pid == QnxPid::PID_PROC || pid == 0) {
        pause();
        ctx.set_syscall_error(Qnx::QEINTR);
    } else {
        ctx.set_syscall_error(Qnx::QESRCH);
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

    Msg msg(Process::current(), send_parts, FarPointer(ds, send_data),  recv_parts, FarPointer(ds, recv_data), B32);
    
    MsgContext info;
    info.m_ctx = &ctx;
    info.m_msg = &msg;
    info.m_proc = proc;
    info.m_via_fd = false;
    info.m_pid = pid;

    proc->handle_msg(info);
    ctx.set_syscall_ok();
}

void Emu::syscall_kill(GuestContext &ctx)
{
    int qnx_signo = ctx.reg_ebx();
    int32_t qnx_code = ctx.reg_edx();
    kill(ctx, qnx_signo, qnx_code);
}

void Emu::kill(GuestContext& ctx, int qnx_signo, int qnx_code)
{
    int host_signo = QnxSigset::map_sig_qnx_to_host(qnx_signo);
    if (host_signo == -1) {
        Log::print(Log::UNHANDLED, "QNX signal %d unknown\n", qnx_signo);
        ctx.set_syscall_error(Qnx::QEINVAL);
        return;
    }

    int host_code;
    if (qnx_code == 0 || qnx_code == -1) {
        host_code = qnx_code;
    } else if (qnx_code > 0) {
        auto pid_info = ctx.proc()->pids().qnx_valid_host(qnx_code);
        if (!pid_info) {
            ctx.set_syscall_error(Qnx::QESRCH);
            return;
        }
        host_code = pid_info->host_pid();
    } else if (qnx_code < -1) {
        auto pid_info = ctx.proc()->pids().qnx_valid_host(-qnx_code);
        if (!pid_info) {
            ctx.set_syscall_error(Qnx::QESRCH);
            return;
        }
        host_code = - pid_info->host_pid();
    }

    Log::print(Log::SIG ,"syscall_kill host pid %d, signo %d\n", host_code, qnx_signo);
    int r = ::kill(host_code, host_signo);
    if (r < 0) {
        ctx.set_syscall_error(map_errno(errno));
    } else {
        ctx.set_syscall_ok();
    }
}

void Emu::dispatch_syscall16(GuestContext& ctx)
{
    uint16_t syscall = ctx.read<uint16_t>(GuestContext::SS, ctx.reg_esp() + 0);
    try {
        switch (syscall) {
            case 0:
                syscall16_sendmx(ctx);
                break;
            case 1:
                syscall16_receivmx(ctx);
                break;
            case 7:
                syscall16_sigreturn(ctx);
                break;
            case 8:
                syscall_priority(ctx);;
                break;
            case 10:
                syscall_yield(ctx);
                break;
            case 11:
                syscall16_sendfdmx(ctx);
                break;
            default:
                printf("Unknown 16-bit syscall %d\n", syscall);
                ctx.set_syscall_error(Qnx::QENOSYS);
        }
    } catch (const SegmentationFault& e) {
        Log::print(Log::UNHANDLED, "Segfault during message handling: %s\n", e.what());
        ctx.set_syscall_error(Qnx::QEFAULT);
    }
}

void Emu::syscall16_sendfdmx(GuestContext &ctx)
{
    auto proc = Process::current();
    uint16_t args[7];
    ctx.read_stack16(1, 7, args);
    
    uint32_t ds = ctx.reg_ds();
    int fd = args[0];
    uint8_t send_parts = args[1];
    uint8_t recv_parts = args[2];
    FarPointer send_data(args[4], args[3]);
    FarPointer recv_data(args[6], args[5]);

    Msg msg(proc, send_parts, send_data,  recv_parts, recv_data, B16);
    MsgContext info;
    info.m_ctx = &ctx;
    info.m_msg = &msg;
    info.m_proc = proc;
    info.m_via_fd = true;
    info.m_fd = fd;

    proc->handle_msg(info);
    ctx.set_syscall_ok();
}

void Emu::syscall16_receivmx(GuestContext &ctx)
{
    // we do not support messages yet, but slib:pause uses it as e.g. "pause" (=wait for signal)
    Qnx::pid_t pid = ctx.reg_edx();
    uint8_t rcv_parts = ctx.reg_ah();
    GuestPtr rmsg = ctx.reg_ebx();

    if (pid == ctx.proc()->pid() || pid == QnxPid::PID_PROC || pid == 0) {
        pause();
        ctx.set_syscall_error(Qnx::QEINTR);
    } else {
        ctx.set_syscall_error(Qnx::QESRCH);
    }
}

void Emu::syscall16_sendmx(GuestContext &ctx)
{
    auto proc = Process::current();
    uint16_t args[7];
    ctx.read_stack16(1, 7, args);
    
    Qnx::pid_t pid = args[0];
    uint8_t send_parts = args[1];
    uint8_t recv_parts = args[2];
    FarPointer send_data(args[4], args[3]);
    FarPointer recv_data(args[6], args[5]);

    Msg msg(Process::current(), send_parts, send_data,  recv_parts, recv_data, B16);
    
    MsgContext info;
    info.m_ctx = &ctx;
    info.m_msg = &msg;
    info.m_proc = proc;
    info.m_via_fd = false;
    info.m_pid = pid;

    proc->handle_msg(info);
    ctx.set_syscall_ok();
}

void Emu::syscall16_sigreturn(GuestContext &ctx)
{
    m_sigmask = ctx.pop_stack();
    ctx.restore_context();

    ctx.reg_esp() += REDLINE;

    Log::print(Log::SIG, "Return from emulated signal, new mask %x, @sp %x, to %x:%x\n", 
        m_sigmask.m_value, ctx.reg_esp(),
        ctx.reg_cs(), ctx.reg_eip()
    );
}

void Emu::signal_raise(int qnx_sig)
{
    Log::print(Log::SIG, "Raising signal %d\n", qnx_sig);
    m_sigpend.set_qnx_sig(qnx_sig);
}

void Emu::signal_mask(uint32_t change_mask, uint32_t bits)
{
    m_sigmask.modify(QnxSigset(change_mask), QnxSigset(bits));
}

uint32_t Emu::signal_getmask() {
    return m_sigmask.m_value;
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
    ctx.clear_64bit_state();

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

    /* We map the host sigmask to guest sigmask */
    sigset_t current;
    sigprocmask(SIG_SETMASK, nullptr, &current);
    m_sigmask = QnxSigset::map_sigmask_host_to_qnx(current);

    /* We abuse the signal mechanism for a context switch, so that we do not need to use any other mechanism */

    sigdelset(&current, SIGUSR1);
    sigprocmask(SIG_SETMASK, &current, nullptr);

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
    if (handler == Qnx::QSIG_HOLD) {
        m_sigmask.set_qnx_sig(qnx_sig);
        return Qnx::QEOK;
    }

    int host_sig = QnxSigset::map_sig_qnx_to_host(qnx_sig);
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
        } else {
            sa.sa_sigaction = static_handler_generic;
        }
        
        int r = sigaction(host_sig, &sa, NULL);
        if (r != 0) {
            return map_errno(r);
        }
    }

    Log::print(Log::SIG, "Registered signal %d, handler %x\n", qnx_sig, handler);   
    auto si = qnx_sigtab(Process::current(), qnx_sig);
    si->flags = 0;
    si->handler_fn = handler;
    si->mask = mask;
    
    return Qnx::QEOK;
}

Qnx::Sigaction* Emu::qnx_sigtab(Process *proc, int qnx_signo, bool required) {
    assert(qnx_signo >= Qnx::QSIGMIN && qnx_signo <= Qnx::QSIGMAX);
    auto sigtab = Process::current()->m_sigtab;
    if (!sigtab) {
        if (required)
            throw GuestStateException("Sigtab not registered");
        else
            return nullptr;
    }
    return &sigtab->actions[qnx_signo - 1];
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

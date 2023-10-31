#pragma once

#include <signal.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <sys/ucontext.h>

#include "guest_context.h"
#include "qnx/errno.h"
#include "qnx/procenv.h"
#include "qnx_sigset.h"

class Segment;
class Process;
class Msg;

/* Handles the low-level emulation */
class Emu {
public:
    Emu();
    void init();
    void enter_emu();

    int signal_sigact(int qnx_sig, uint32_t handler, uint32_t mask);
    void signal_raise(int qnx_sig);
    void signal_mask(uint32_t change_mask, uint32_t bits);
    uint32_t signal_getmask();

    static void debug_hook_problem();
    static void debug_hook_sig_enter();

    static Qnx::errno_t map_errno(int v);
    
    ~Emu();
private:
    void dispatch_syscall(GuestContext& ctx);
    void syscall_sendmx(GuestContext& ctx);
    void syscall_sendfdmx(GuestContext& ctx);
    void syscall_kill(GuestContext& ctx);
    void syscall_sigreturn(GuestContext& ctx);
    void syscall_receivmx(GuestContext& ctx);
    void syscall_priority(GuestContext& ctx);

    void dispatch_syscall_sem(GuestContext& ctx);

    void handler_segv(int sig, siginfo_t *info, void *uctx);
    void handle_guest_segv(GuestContext &ctx, siginfo_t *info);
    void handler_generic(int sig, siginfo_t *info, void *uctx);
    void signal_tail(GuestContext& ctx);
    void sync_host_sigmask(GuestContext &ctx);

    static bool is_special_sighandler(uint32_t qnx_handler);
    static Qnx::Sigaction* qnx_sigtab(Process *proc, int qnx_signo, bool required=true);

    static void static_handler_segv(int sig, siginfo_t *info, void *uctx);
    static void static_handler_user(int sig, siginfo_t *info, void *uctx);
    static void static_handler_generic(int sig, siginfo_t *info, void *uctx);

    static bool matches_syscall(GuestContext &ctx, int int_nr, int *insn_len);

    TlsFixup m_tls_fixup;

    std::shared_ptr<Segment> m_emulation_stack;

    /* We need to emulate the masks and pending bits, we cannot directly use the host signals, because
     * a) we must raise the simulated signal always when leaving the outermost signal (hence sigpend)
     * b) calling sigprocmask is useless, since it will be restored when exiting signal
     */
    QnxSigset m_sigpend;
    QnxSigset m_sigmask;

    static constexpr int REDLINE = 128;
};


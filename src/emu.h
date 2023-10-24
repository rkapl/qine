#pragma once

#include <signal.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <sys/ucontext.h>
#include <bitset>

#include "guest_context.h"
#include "qnx/errno.h"
#include "qnx/procenv.h"

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

    void handler_segv(int sig, siginfo_t *info, void *uctx);
    void handler_generic(int sig, siginfo_t *info, void *uctx);
    void signal_tail(GuestContext& ctx);

    static void static_handler_segv(int sig, siginfo_t *info, void *uctx);
    static void static_handler_user(int sig, siginfo_t *info, void *uctx);
    static void static_handler_generic(int sig, siginfo_t *info, void *uctx);

    static int map_sig_host_to_qnx(int sig);
    static int map_sig_qnx_to_host(int sig);
    static sigset_t map_sigmask_qnx_to_host(uint32_t mask);

    TlsFixup m_tls_fixup;

    std::shared_ptr<Segment> m_emulation_stack;

    /* We need to emulate the masks and pending bits */
    uint32_t m_sigpend;
    uint32_t m_sigmask;
};


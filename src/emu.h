#pragma once

#include <signal.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <sys/ucontext.h>
#include <bitset>

#include "context.h"
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

    static void debug_hook();

    static Qnx::errno_t map_errno(int v);
    
    ~Emu();
private:
    void dispatch_syscall(Context& ctx);
    void syscall_sendmx(Context& ctx);
    void syscall_sendfdmx(Context& ctx);
    void syscall_kill(Context& ctx);
    void syscall_sigreturn(Context& ctx);

    void handler_segv(int sig, siginfo_t *info, void *uctx);
    void handler_generic(int sig, siginfo_t *info, void *uctx);
    void signal_tail(Context& ctx);

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


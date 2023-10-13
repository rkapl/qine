#pragma once

#include <signal.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <sys/ucontext.h>

#include "context.h"
#include "qnx/errno.h"

class Segment;
class Process;
class Msg;

/* Handles the low-level emulation */
class Emu {
public:
    Emu();
    void init();
    void enter_emu();
    static void debug_hook();

    static Qnx::errno_t map_errno(int v);
    
    ~Emu();
private:
    void dispatch_syscall(Context& ctx);
    void syscall_sendmx(Context& ctx);
    void syscall_sendfdmx(Context& ctx);

    void handler_segv(int sig, siginfo_t *info, void *uctx);
    static void static_handler_segv(int sig, siginfo_t *info, void *uctx);
    static void static_handler_user(int sig, siginfo_t *info, void *uctx);

    TlsFixup m_tls_fixup;

    std::shared_ptr<Segment> m_stack;
};


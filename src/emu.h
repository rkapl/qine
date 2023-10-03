#pragma once

#include <signal.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <sys/ucontext.h>

#include "context.h"

class Segment;
class Process;
class Msg;

/* Handles the low-level emulation */
class Emu {
public:
    Emu();
    void init();
    void enter_emu();
    ~Emu();
private:
    void dispatch_syscall(Context& ctx);
    void syscall_sendmx(Context& ctx);
    void syscall_sendfdmx(Context& ctx);
    void dispatch_msg(int pid, Context& ctx, Msg& msg);
    void msg_handle(Msg &ctx);

    void proc_segment_realloc(Msg &ctx);
    void proc_terminate(Msg &ctx);

    void io_read(Msg &ctx);
    void io_write(Msg &ctx);
    void io_lseek(Msg &ctx);

    void handler_segv(int sig, siginfo_t *info, void *uctx);
    static void static_handler_segv(int sig, siginfo_t *info, void *uctx);
    static void static_handler_user(int sig, siginfo_t *info, void *uctx);
    static void debug_hook();

    TlsFixup m_tls_fixup;

    std::shared_ptr<Segment> m_stack;
};


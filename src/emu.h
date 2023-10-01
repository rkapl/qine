#pragma once

#include <signal.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <sys/ucontext.h>

class Segment;
class Process;
class Msg;
class Context;

/* Handles the low-level emulation */
class Emu {
public:
    Emu();
    void init();
    void enter_emu();
    ~Emu();
private:
    void dispatch_syscall(Context& ctx);
    void syscall_sendmx(Context &ctx);
    void msg_proc(Msg &ctx);

    void handler_segv(int sig, siginfo_t *info, void *uctx);
    static void static_handler_segv(int sig, siginfo_t *info, void *uctx);
    static void static_handler_user(int sig, siginfo_t *info, void *uctx);
    static void debug_hook();

    std::shared_ptr<Segment> m_stack;
};


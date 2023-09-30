#pragma once

#include "memory.h"
#include <bits/types/FILE.h>
#include <bits/types/siginfo_t.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <sys/ucontext.h>

class Segment;
class Process;

/* Handles the low-level emulation */
class Emu {
public:
    Emu();
    void init();
    void enter_emu();
    ~Emu();
private:
    static void handler_segv(int sig, siginfo_t *info, void *uctx);
    static void handler_user(int sig, siginfo_t *info, void *uctx);
    static void debug_hook();

    std::shared_ptr<Segment> m_stack;
};


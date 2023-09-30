#include <array>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/ucontext.h>
#include <unistd.h>

#include "emu.h"
#include "mem_ops.h"
#include "process.h"
#include "segment.h"
#include "types.h"
#include "context.h"

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
    sas.ss_sp = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_stack->location()) +MemOps::PAGE_SIZE);
    sas.ss_flags = 0;
    if (sigaltstack(&sas, nullptr) != 0) {
        throw std::runtime_error(strerror(errno));
    }

    struct sigaction sa = {};
    sa.sa_sigaction = Emu::handler_segv;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, nullptr) == -1){
        throw std::runtime_error(strerror(errno));
    }
}

void Emu::handler_segv(int sig, siginfo_t *info, void *uctx_void)
{
    auto ctx = Context(reinterpret_cast<ucontext_t*>(uctx_void));
    
    ctx.dump(stdout);
    debug_hook();
}

void Emu::debug_hook() {

}

void Emu::handler_user(int sig, siginfo_t *info, void *uctx)
{
    auto ctx = reinterpret_cast<ucontext_t*>(uctx);
    auto proc = Process::current();

    std::array<int, 13> transfer_regs = {
        REG_EIP,
        REG_CS,
        REG_SS,
        REG_ESP,
        REG_DS,
        REG_ES,
        REG_FS,
        REG_EAX,
        REG_EBX,
        REG_ECX,
        REG_EDX,
        REG_ESI,
        REG_EDI,
    };

    proc->startup_context.uc_mcontext.gregs[REG_EIP] = 0x7d0;

    for (int reg: transfer_regs) {
        ctx->uc_mcontext.gregs[reg] = proc->startup_context.uc_mcontext.gregs[reg];
    }
}

void Emu::enter_emu() {

    /* We abuse the signal mechanism for a context switch*/
    struct sigaction sa;
    sa.sa_sigaction = Emu::handler_user;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, nullptr)) {
        throw std::runtime_error(strerror(errno));
    }
    kill(getpid(), SIGUSR1);
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
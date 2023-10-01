#include <array>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include "emu.h"
#include "mem_ops.h"
#include "process.h"
#include "qnx/errno.h"
#include "qnx/magic.h"
#include "qnx/types.h"
#include "qnx/msg.h"
#include "msg/dump.h"
#include "gen_msg/proc.h"
#include "segment.h"
#include "types.h"
#include "context.h"
#include "msg.h"

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
}

void Emu::static_handler_segv(int sig, siginfo_t *info, void *uctx_void) {
    Process::current()->m_emu.handler_segv(sig, info, uctx_void);
}

void Emu::handler_segv(int sig, siginfo_t *info, void *uctx_void)
{
    auto ctx = Context(reinterpret_cast<ucontext_t*>(uctx_void));
    auto eip = ctx.reg(REG_EIP);
    bool handled = false;

    if (ctx.reg(REG_ES) == Qnx::MAGIC_PTR_SELECTOR) {
        // hack: migrate to LDT
        ctx.reg(REG_ES) = Qnx::MAGIC_PTR_SELECTOR | 4;
        printf("Migrating to LDT @ %x\n", ctx.reg(REG_EIP));
        handled = true;
    }

    if (ctx.read<uint8_t>(Context::CS, eip) == 0xCD && ctx.read<uint8_t>(Context::CS, eip + 1) == 0xF2) {
        dispatch_syscall(ctx);
        ctx.reg(REG_EIP) += 2;
        handled = true;
    }
    
    if (!handled) {
        ctx.dump(stdout);
        debug_hook();
    }
}

void Emu::dispatch_syscall(Context& ctx)
{
    uint8_t syscall = ctx.reg_al();
    switch (syscall) {
        case 0:
            syscall_sendmx(ctx);
            break;
        default:
            printf("Unknown syscall %d\n", syscall);
            ctx.proc()->set_errno(Qnx::QENOSYS);
            ctx.reg(REG_EAX) = -1;
    }
}

void Emu::syscall_sendmx(Context &ctx)
{
    uint32_t ds= ctx.reg(REG_DS);
    Qnx::pid_t pid = ctx.reg(REG_EDX);
    uint8_t send_parts = ctx.reg_ah();
    uint8_t recv_parts = ctx.reg_ch();
    GuestPtr send_data = ctx.reg(REG_EBX);
    GuestPtr recv_data = ctx.reg(REG_ESI);

    Msg msg(Process::current(), send_parts, FarPointer(ds, send_data),  recv_parts, FarPointer(ds, recv_data));

    switch (pid) {
    case 1:
        msg_proc(msg);
        break;
    default:
        printf("Unknown message destination %d\n", pid);
        ctx.proc()->set_errno(Qnx::QESRCH);
        ctx.reg(REG_EAX) = -1;
    }

}

void Emu::msg_proc(Msg& msg) {
    Qnx::MsgHeader hdr;
    msg.read_type(&hdr);

    printf("Msg to proc dec %d : %d\n", hdr.type, hdr.subtype);
    QnxMsg::dump_message(stdout, QnxMsg::proc::list, msg);
}

void Emu::debug_hook() {

}

void Emu::static_handler_user(int sig, siginfo_t *info, void *uctx)
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
    printf("Entering emulation\n");
}

void Emu::enter_emu() {
    struct sigaction sa = {};
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
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <string>
#include <sys/ucontext.h>
#include <unistd.h>
#include <asm/prctl.h>
#include <sys/syscall.h>

#include "context.h"
#include "process.h"
#include "types.h"

Context::Context(ucontext_t *ctx, ExtraContext *ectx): m_ctx(ctx), m_ectx(ectx), m_proc(Process::current()) {}

void* Context::translate(SegmentRegister s, uint32_t addr, uint32_t size, RwOp write)
{
    return m_proc->translate_segmented(FarPointer(get_seg(s), addr), size, write);
}

void Context::read_data(SegmentRegister s, void *dst, uint32_t addr, uint32_t size)
{

    auto linear = m_proc->translate_segmented(FarPointer(get_seg(s), addr), size, RwOp::READ);
    memcpy(dst, linear, size);
}

void Context::write_data(SegmentRegister s, const void *src, uint32_t addr, uint32_t size)
{
    auto linear = m_proc->translate_segmented(FarPointer(get_seg(s), addr), size, RwOp::WRITE);
    memcpy(linear, src, size);
}

uint16_t Context::get_seg(SegmentRegister r) {
    switch (r) {
        default:
        case CS:
            return reg_cs();
        case DS:
            return reg_ds();
        case ES:
            return reg_es();
        case FS:
            return reg_fs();
        case SS:
            return reg_ss();
    }
}

void Context::push_stack(uint32_t value) {
    reg_esp() -= 4;
    write(SS, reg_esp(), value);
}

uint32_t Context::pop_stack() {
    auto v = read<uint32_t>(SS, reg_esp());
    reg_esp() += 4;
    return v;
}

void Context::dump(FILE *s) {
    auto cs = reg_cs();
    auto ip = reg_eip();
    try {
        auto linear = translate(Context::CS, ip, 0);
        fprintf(s, "handler_segv %x:%x, linear %p\n", cs, ip, linear);
    } catch (const GuestStateException& e) {
        fprintf(s, "handler_segv %x:%x, %s\n", cs, ip, e.what());
    }

    fprintf(s, "EAX: %08X  EBX: %08x  ECX: %08X  EDX: %08X\n",
        reg_eax(), reg_ebx(), reg_ecx(), reg_edx()
    );

    fprintf(s, "ESI: %08x  EDI: %08X  EBP: %08X\n",
        reg_esi(), reg_edi(), reg_ebp()
    );

    fprintf(s, "ESP: %08X  EFL: %08X  EIP: %08X\n",
        reg_esp(), reg_eflags(), reg_eip()
    );

    fprintf(s, "CS: %04X  SS: %04X  DS: %04X  ES: %04X FS: %04X GS: %04X\n",
        reg_cs(), reg_ss(), reg_ds(), reg_es(), reg_fs(), reg_gs()
    );

    fprintf(s, "-- stack --\n");
    for (int i = 0; i < 17; i++) {
        auto addr = reg_esp() + i * 4;
        fprintf(s, "%08X: %08X\n", addr, read<uint32_t>(SS, addr));
    }

    #if 0
    fprintf(s, "-- env --\n");
    auto m_ptr = m_proc->m_magic_guest_pointer;
    auto user_ptr = [m_ptr] (void* ptr) {
        return FarPointer(m_ptr.m_segment, reinterpret_cast<uint32_t>(ptr));
    };
    auto m = m_proc->m_magic;
    auto cwd = read_string(user_ptr(m->cwd));
    auto root = read_string(user_ptr(m->root_prefix));
    auto cmd = read_string(user_ptr(m->cmd));
    
    fprintf(s, "cmd=%s, cwd=%s, root=%s\n", cmd.c_str(), cwd.c_str(), root.c_str());
    #endif
}

std::string Context::read_string(FarPointer ptr, size_t size) {
    std::string acc;
    auto mem = static_cast<const char*>(m_proc->translate_segmented(ptr, size, RwOp::READ));
    for (size_t i = 0; i < size; i++) {
        if (mem[i] == 0)
            break;
        acc.push_back(mem[i]);
    }
    return acc;
}

void ExtraContext::from_cpu() {
    __asm__ ("mov %%ds, %0": "=r" (ds));
    __asm__ ("mov %%es, %0": "=r" (es));
}

void ExtraContext::to_cpu() {
    __asm__ ("mov %0, %%ds":: "r" (ds));
    __asm__ ("mov %0, %%es":: "r" (es));
}


void TlsFixup::save() {
    #ifdef __amd64__
    int r;
    /* There are optionally some instructions for that*/
    r = syscall(SYS_arch_prctl, ARCH_GET_FS, &fsbase);
    if (r < 0)
        throw std::logic_error("arch_prctl failed");
    r = syscall(SYS_arch_prctl, ARCH_SET_GS, &gsbase);
    if (r < 0)
        throw std::logic_error("arch_prctl failed");
    #endif
}
void TlsFixup::restore() {
    #ifdef __amd64__
    int r;
    r = syscall(SYS_arch_prctl, ARCH_SET_FS, fsbase);
    if (r < 0)
        throw std::logic_error("arch_prctl failed");
    r = syscall(SYS_arch_prctl, ARCH_SET_GS, gsbase);
    if (r < 0)
        throw std::logic_error("arch_prctl failed");
    #endif
}
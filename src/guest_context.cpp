#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <string>
#include <sys/ucontext.h>
#include <sys/wait.h>
#include <unistd.h>
#include <asm/prctl.h>
#include <sys/syscall.h>

#include "compiler.h"
#include "guest_context.h"
#include "process.h"
#include "types.h"

GuestContext::GuestContext(ucontext_t *ctx, ExtraContext *ectx): m_ctx(ctx), m_ectx(ectx), m_proc(Process::current()) {
    m_b16 = m_proc->m_bits == B16;
}

void* GuestContext::translate(SegmentRegister s, uint32_t addr, uint32_t size, RwOp write)
{
    return m_proc->translate_segmented(FarPointer(get_seg(s), addr), size, write);
}

void GuestContext::read_data(SegmentRegister s, void *dst, uint32_t addr, uint32_t size)
{

    auto linear = m_proc->translate_segmented(FarPointer(get_seg(s), addr), size, RwOp::READ);
    memcpy(dst, linear, size);
}

size_t GuestContext::read_data_partial(SegmentRegister s, void *dst, uint32_t addr, uint32_t size)
{
    auto seg = m_proc->descriptor_by_selector(get_seg(s))->segment();
    if (addr >= seg->size()) {
        throw SegmentationFault("Read past segment end");
    }

    auto effective_size = std::min(size, addr - static_cast<uint32_t>(seg->size()));
    auto linear = seg->pointer(addr, effective_size);
    memcpy(dst, linear, size);
    return effective_size;
}

void GuestContext::write_data(SegmentRegister s, const void *src, uint32_t addr, uint32_t size)
{
    auto linear = m_proc->translate_segmented(FarPointer(get_seg(s), addr), size, RwOp::WRITE);
    memcpy(linear, src, size);
}

uint16_t GuestContext::get_seg(SegmentRegister r) {
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

void GuestContext::push_stack(uint32_t value) {
    reg_esp() -= 4;
    write(SS, reg_esp(), value);
}

void GuestContext::push_stack16(uint16_t value) {
    reg_esp() -= 2;
    write(SS, reg_esp(), value);
}

uint32_t GuestContext::pop_stack() {
    auto v = read<uint32_t>(SS, reg_esp());
    reg_esp() += 4;
    return v;
}

uint16_t GuestContext::pop_stack16() {
    auto v = read<uint16_t>(SS, reg_esp());
    reg_esp() += 2;
    return v;
}

void GuestContext::dump(FILE *s, size_t stack) {
    auto cs = reg_cs();
    auto ip = reg_eip();
    try {
        auto linear = translate(GuestContext::CS, ip, 0);
        fprintf(s, "ip %x:%x, linear %p\n", cs, ip, linear);
    } catch (const GuestStateException& e) {
        fprintf(s, "ip %x:%x, %s\n", cs, ip, e.what());
    } catch (const SegmentationFault&) {
        fprintf(s, "ip %x:%x, linear unavailable\n", cs, ip);
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

    fprintf(s, "CR2: %04X TRAPNO: %04X ERR: %04X\n", greg(REG_CR2), greg(REG_TRAPNO), greg(REG_ERR));

    if (stack == 0)
        stack = 16;

    fprintf(s, "-- code --\n");
    {
        char code_buf[50];
        // TODO: handle short reads (execution near segment end)
        size_t code_size = read_data_partial(CS, code_buf, reg_eip(), sizeof(code_buf));
        auto code_pipe = UniqueFd::create_pipe();
        if (!code_pipe.first.valid() ) {
            fprintf(s, "Cannot create pipe: %s\n", strerror(errno));
        } else {
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(s, "Cannot fork to run ndisasm: %s\n", strerror(errno));
            } else if (pid == 0) {
                // bit hacky for now, we should use our own pipe back and write it to s
                code_pipe.second.close();
                dup2(code_pipe.first.get(), 0);
                dup2(fileno(s), 1);
                dup2(fileno(s), 2);
                execlp("ndisasm", "ndisasm", 
                    "-o", std::to_string(reg_eip()).c_str(), 
                    "-b", (m_proc->m_bits == B16) ? "16" : "32",
                "-", NULL);
                fprintf(s, "Cannot run ndisasm: %s\n", strerror(errno));
            } else {
                code_pipe.first.close();
                // we hope here that the pipe buffer in OS is big enough not to give us partial writes
                ::write(code_pipe.second.get(), code_buf, code_size);
                code_pipe.second.close();
                waitpid(pid, NULL, 0);
            }
        }
    }

    fprintf(s, "-- stack --\n");
    for (int i = 0; i < stack; i++) {
        if (m_b16) {
            auto addr = reg_esp() + i * 2;
            fprintf(s, "%04X: %04X\n", addr, read<uint16_t>(SS, addr));
        } else {
            auto addr = reg_esp() + i * 4;
            fprintf(s, "%08X: %08X\n", addr, read<uint32_t>(SS, addr));
        }
    }

    #if 1
    if (!m_b16) {
        fprintf(s, "-- magic --\n");
        auto m_ptr = m_proc->m_magic_guest_pointer;
        auto user_ptr = [m_ptr] (GuestPtr ptr) {
            return FarPointer(m_ptr.m_segment, ptr);
        };
        auto m = m_proc->m_magic;
        auto cwd = read_string(user_ptr(m->cwd));
        auto root = read_string(user_ptr(m->root_prefix));
        
        fprintf(s, "exec=%s\n", m_proc->executed_file().qnx_path());
        fprintf(s, "cwd=%s, root=%s, pid=%d, ppid=%d, nid=%d\n", 
            cwd.c_str(), root.c_str(),
            m->my_pid, m->dads_pid, m->my_nid
        );
    }
    #endif
}

void GuestContext::save_context()
{
    auto esp = reg_esp();
    /* pushad */
    push_stack(reg_eax());
    push_stack(reg_ecx());
    push_stack(reg_edx());
    push_stack(reg_ebx());
    push_stack(esp);
    push_stack(reg_ebp());
    push_stack(reg_esi());
    push_stack(reg_edi());

    push_stack(reg_eflags());
    push_stack(reg_cs());
    push_stack(reg_eip());
    push_stack(reg_ds());
    push_stack(reg_es());
    push_stack(reg_gs());
    push_stack(reg_ss());
}

void GuestContext::restore_context()
{
    reg_ss() = pop_stack();
    reg_gs() = pop_stack();
    reg_es() = pop_stack();
    reg_ds() = pop_stack();
    reg_eip() = pop_stack();
    reg_cs() = pop_stack();
    reg_eflags() = pop_stack();

    reg_edi() = pop_stack();
    reg_esi() = pop_stack();
    reg_ebp() = pop_stack();
    /* esp = */ pop_stack();
    reg_ebx() = pop_stack();
    reg_edx() = pop_stack();
    reg_ecx() = pop_stack();
    reg_eax() = pop_stack();
}

std::string GuestContext::read_string(FarPointer ptr, size_t size) {
    std::string acc;
    try {
        auto mem = static_cast<const char*>(m_proc->translate_segmented(ptr, size, RwOp::READ));
        for (size_t i = 0; i < size; i++) {
            if (mem[i] == 0)
                break;
            acc.push_back(mem[i]);
        }
    } catch (const GuestStateException& x) {
        acc = "<unreadable>";
    }
    return acc;
}

void GuestContext::clear_64bit_state() {
   m_ctx->uc_mcontext.gregs[REG_RIP] = 0;
   m_ctx->uc_mcontext.gregs[REG_RDI] = 0;
   m_ctx->uc_mcontext.gregs[REG_RSI] = 0;
   m_ctx->uc_mcontext.gregs[REG_RBP] = 0;
   m_ctx->uc_mcontext.gregs[REG_RSP] = 0;
   m_ctx->uc_mcontext.gregs[REG_RBX] = 0;
   m_ctx->uc_mcontext.gregs[REG_RDX] = 0;
   m_ctx->uc_mcontext.gregs[REG_RCX] = 0;
   m_ctx->uc_mcontext.gregs[REG_RAX] = 0;
}

qine_no_tls void ExtraContext::from_cpu() {
    __asm__ ("mov %%ds, %0": "=r" (ds));
    __asm__ ("mov %%es, %0": "=r" (es));
    __asm__ ("mov %%fs, %0": "=r" (fs));
    __asm__ ("mov %%gs, %0": "=r" (gs));
}

qine_no_tls void ExtraContext::to_cpu() {
    __asm__ ("mov %0, %%ds":: "r" (ds));
    __asm__ ("mov %0, %%es":: "r" (es));
    __asm__ ("mov %0, %%fs":: "r" (fs));
    __asm__ ("mov %0, %%gs":: "r" (gs));
}


void TlsFixup::save() {
    #ifdef __amd64__
    int r;
    /* There are optionally some instructions for that*/
    r = syscall(SYS_arch_prctl, ARCH_GET_FS, &fsbase);
    if (r < 0)
        throw std::logic_error("arch_prctl failed");
    r = syscall(SYS_arch_prctl, ARCH_GET_GS, &gsbase);
    if (r < 0)
        throw std::logic_error("arch_prctl failed");
    #endif
}
qine_no_tls void TlsFixup::restore() {
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

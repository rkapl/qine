#include "context.h"
#include "process.h"
#include <cstdint>
#include <cstdio>
#include <sys/ucontext.h>
#include <unistd.h>

Context::Context(ucontext_t *ctx): m_ctx(ctx), m_proc(Process::current()) {}

void* Context::translate(SegmentRegister s, uint32_t addr, uint32_t size, bool write)
{
    return m_proc->translate_segmented(get_seg(s), addr, size, write);
}

void Context::read_data(SegmentRegister s, void *dst, uint32_t addr, uint32_t size)
{

    auto linear = m_proc->translate_segmented(get_seg(s), addr, size, false);
    memcpy(dst, linear, size);
}

void Context::write_data(SegmentRegister s, const void *src, uint32_t addr, uint32_t size)
{
    auto linear = m_proc->translate_segmented(get_seg(s), addr, size, true);
    memcpy(linear, src, size);
}

uint16_t Context::get_seg(SegmentRegister r) {
    switch (r) {
        default:
        case CS:
            return greg(REG_CS);
        case DS:
            return greg(REG_DS);
        case ES:
            return greg(REG_ES);
        case FS:
            return greg(REG_FS);
        case SS:
            return greg(REG_SS);
    }
}

void Context::push_stack(uint32_t value) {
    greg(REG_ESP) -= 4;
    write(SS, greg(REG_ESP), value);
}

uint32_t Context::pop_stack() {
    auto v = read<uint32_t>(SS, greg(REG_ESP));
    greg(REG_ESP) += 4;
    return v;
}

void Context::dump(FILE *s) {
    auto cs = greg(REG_CS);
    auto ip = greg(REG_EIP);
    try {
        auto linear = translate(Context::CS, ip, 0);
        fprintf(s, "handler_segv %x:%x, linear %p\n", cs, ip, linear);
    } catch (const GuestStateException& e) {
        fprintf(s, "handler_segv %x:%x, %s\n", cs, ip, e.what());
    }

    fprintf(s, "EAX: %08X  EBX: %08x  ECX: %08X  EDX: %08X\n",
        greg(REG_EAX), greg(REG_EBX), greg(REG_ECX), greg(REG_EDX)
    );

    fprintf(s, "EDX: %08X  ESI: %08x  EDI: %08X  EBP: %08X\n",
        greg(REG_EDX), greg(REG_ESI), greg(REG_EDI), greg(REG_EBP)
    );

    fprintf(s, "ESP: %08X  EFL: %08X  EIP: %08X\n",
        greg(REG_ESP), greg(REG_EFL), greg(REG_EIP)
    );

    fprintf(s, "CS: %04X  SS: %04X  DS: %04X  ES: %04X FS: %04X GS: %04X\n",
        greg(REG_CS), greg(REG_SS), greg(REG_DS), greg(REG_ES), greg(REG_FS), greg(REG_GS)
    );

    fprintf(s, "-- stack --\n");
    for (int i = 0; i < 17; i++) {
        auto addr = greg(REG_ESP) + i * 4;
        fprintf(s, "%08X: %08X\n", addr, read<uint32_t>(SS, addr));
    }
}
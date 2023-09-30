#pragma once

#include "emu.h"
#include <stdexcept>
#include <sys/ucontext.h>

class Process;

class GuestStateException: public std::runtime_error {
public:
    GuestStateException(const char *what): std::runtime_error(what) {}
};

/* Wrapper around ucontext_t with helpers */
struct Context {
public:
    enum SegmentRegister {
        CS, DS, ES, FS, SS
    };

    Context(ucontext_t *ctx);

    void* translate(SegmentRegister, uint32_t addr, uint32_t size = 0, bool write = false);
    void read_data(SegmentRegister s, void *dst, uint32_t addr, uint32_t size);
    void write_data(SegmentRegister s, const void *src, uint32_t addr, uint32_t size);

    template<class T>
    T read(SegmentRegister s, uint32_t addr);

    template<class T>
    void write(SegmentRegister s, uint32_t addr, const T& value);

    void push_stack(uint32_t value);
    uint32_t pop_stack();

    void dump(FILE *stream);

    inline greg_t& greg(int reg);

    ucontext_t *m_ctx;
private:
    uint16_t get_seg(SegmentRegister r);

    Process *m_proc;
};

template<class T>
T Context::read(SegmentRegister s, uint32_t addr) {
    T tmp;
    read_data(s, &tmp, addr, sizeof(&tmp));
    return tmp;
}

template<class T>
void Context::write(SegmentRegister s, uint32_t addr, const T& value)
{
    write_data(s, &value, addr, sizeof(T));
}

greg_t& Context::greg(int greg) {
    return m_ctx->uc_mcontext.gregs[greg];
}
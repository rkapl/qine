#pragma once

#include "qnx/errno.h"
#include "types.h"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <signal.h>
#include <sys/types.h>
#include <sys/ucontext.h>

class Process;

/* Exception thrown when the guest does something we really do not want to handle */
class GuestStateException: public std::runtime_error {
public:
    GuestStateException(const char *what): std::runtime_error(what) {}
};

/* We tried to access inaccesible memory, e.g. when reading message. This is often propagate as error code. */
class SegmentationFault: public std::runtime_error {
public:
    SegmentationFault(const char *what): std::runtime_error(what) {}
};

/* x64 does not restore FS and GS to known values on signal entry, we need to reload them */
class TlsFixup {
public:
    void save();
    void restore();
private:
    uint64_t fsbase;
    uint64_t gsbase;
};

/* DS, ES, FS and GS are not stored in ucontext on 64 bit linux, store them here. */
class ExtraContext {
public:
    void from_cpu();
    void to_cpu();

    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
};

/* Wrapper around ucontext_t with helpers */
struct GuestContext {
public:
    enum SegmentRegister {
        CS, DS, ES, FS, SS
    };

    GuestContext(ucontext_t *ctx, ExtraContext *exct);
    GuestContext(const GuestContext&) = default;
    GuestContext& operator=(const GuestContext&) = default;
    GuestContext() {}

    void* translate(SegmentRegister, uint32_t addr, uint32_t size = 0, RwOp write = RwOp::READ);
    void read_data(SegmentRegister s, void *dst, uint32_t addr, uint32_t size);
    void write_data(SegmentRegister s, const void *src, uint32_t addr, uint32_t size);

    template<class T>
    T read(SegmentRegister s, uint32_t addr);

    template<class T>
    void write(SegmentRegister s, uint32_t addr, const T& value);

    void push_stack(uint32_t value);
    uint32_t pop_stack();

    // Save/restore registers to stack in QNX format, can be used for e.g. stack handlers
    void save_context();
    void restore_context();

    void dump(FILE *stream, size_t stack = 0);
    std::string read_string(FarPointer ptr, size_t size = 256);

    inline greg_t& reg(int reg);
    inline greg_t reg(int reg) const;

    ucontext_t *m_ctx;
    ExtraContext *m_ectx;
    inline Process *proc() const;
    
    void clear_64bit_state();

    /* Register accessors */
    /* We are on x86 so we can afford some unaligned stuff */
    #ifdef __amd64__
    inline uint32_t& reg_eax() { return greg(REG_RAX); };
    inline uint32_t& reg_ebx() { return greg(REG_RBX); };
    inline uint32_t& reg_ecx() { return greg(REG_RCX); };
    inline uint32_t& reg_edx() { return greg(REG_RDX); };
    inline uint32_t& reg_edi() { return greg(REG_RDI); };
    inline uint32_t& reg_esi() { return greg(REG_RSI); };
    inline uint32_t& reg_ebp() { return greg(REG_RBP); };
    inline uint32_t& reg_esp() { return greg(REG_RSP); };
    inline uint32_t& reg_eip() { return greg(REG_RIP); };
    inline uint32_t& reg_eflags() { return greg(REG_EFL); };
    inline uint16_t& reg_cs() { return greg_shift<uint16_t>(REG_CSGSFS, 0);};
    inline uint16_t& reg_ds() { return m_ectx->ds;};
    inline uint16_t& reg_es() { return m_ectx->es;};
    inline uint16_t& reg_gs() { return greg_shift<uint16_t>(REG_CSGSFS, 16);};
    inline uint16_t& reg_fs() { return greg_shift<uint16_t>(REG_CSGSFS, 32);};
    inline uint16_t& reg_ss() { return greg_shift<uint16_t>(REG_CSGSFS, 48);};

    inline size_t reg_trapno() { return greg(REG_TRAPNO); };
    inline sigset_t& saved_sigmask() { return m_ctx->uc_sigmask; };

    inline uint8_t&  reg_al()  { return greg_shift<uint8_t>(REG_RAX, 0); }
    inline uint8_t&  reg_ah()  { return greg_shift<uint8_t>(REG_RAX, 8); }
    inline uint8_t&  reg_cl()  { return greg_shift<uint8_t>(REG_RCX, 0); }
    inline uint8_t&  reg_ch()  { return greg_shift<uint8_t>(REG_RCX, 8); }

    #else
    #error 32bit unsupported for now
    #endif

    inline void set_carry() { reg_eflags() |= 1; }
    inline void clear_carry() { reg_eflags() &=  ~1u; }
    inline void set_syscall_error(int qnx_errno) {
        reg_eax() = qnx_errno;
        set_carry();
    }
    inline void set_syscall_ok() {
        reg_eax() = Qnx::QEOK;
    }

private:
    #ifdef __amd64__
    inline uint32_t& greg(int id) {
        return reinterpret_cast<uint32_t&>(m_ctx->uc_mcontext.gregs[id]); 
    }
    template <class T>
    inline T& greg_shift(int id, uint8_t shift) { 
        uint8_t *pos = reinterpret_cast<uint8_t*>(&m_ctx->uc_mcontext.gregs[id]);
        return *reinterpret_cast<T*>(pos + shift / 8);
    }
    #else
    #error 32bit unsupported for now
    #endif

    uint16_t get_seg(SegmentRegister r);

    Process *m_proc;
};

template<class T>
T GuestContext::read(SegmentRegister s, uint32_t addr) {
    T tmp;
    read_data(s, &tmp, addr, sizeof(T));
    return tmp;
}

template<class T>
void GuestContext::write(SegmentRegister s, uint32_t addr, const T& value)
{
    write_data(s, &value, addr, sizeof(T));
}

Process *GuestContext::proc() const {
    return m_proc;
}

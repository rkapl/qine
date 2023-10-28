#pragma once

#include "qnx/signal.h"
#include <stdint.h>
#include <signal.h>
#include <assert.h>

struct QnxSigset {
    // Uses the QNX-compatible sigset, that is signal mask is (1u << (signo -1)))
    QnxSigset() = default;
    constexpr QnxSigset(uint32_t value): m_value(value) {}

    inline void set_qnx_sig(int signo);
    inline void clear_qnx_sig(int signo);
    inline bool operator[](int signo);
    inline void modify(QnxSigset mask, QnxSigset bits);
    inline int find_first();
    bool is_empty() const { return m_value == 0; }

    static int map_sig_host_to_qnx(int sig);
    static int map_sig_qnx_to_host(int sig);
    sigset_t map_to_host_sigset() const;
    static QnxSigset map_sigmask_host_to_qnx(const sigset_t& set);
    
    static constexpr QnxSigset empty() {return QnxSigset(0); };

    uint32_t m_value;
private:
    inline static uint32_t get_qnx_mask(int signo);
};

uint32_t QnxSigset::get_qnx_mask(int signo) {
    assert(signo >= Qnx::QSIGMIN && signo <= Qnx::QSIGMAX);
    return 1u << (signo - Qnx::QSIGMIN);
}

void QnxSigset::set_qnx_sig(int signo) {
    m_value |= get_qnx_mask(signo);
}
void QnxSigset::clear_qnx_sig(int signo) {
    m_value &= ~get_qnx_mask(signo);
}

bool QnxSigset::operator[](int signo) {
    return m_value & get_qnx_mask(signo);
}

void QnxSigset::modify(QnxSigset mask, QnxSigset bits) {
    m_value = (m_value & ~mask.m_value) | (bits.m_value & mask.m_value);
}

inline int QnxSigset::find_first() {
    int qnx_sig;
    for (qnx_sig = Qnx::QSIGMIN; qnx_sig <= Qnx::QSIGMAX; qnx_sig++) {
        if((*this)[qnx_sig])
            return qnx_sig;
    }
    return -1;
}
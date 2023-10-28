#include "qnx_sigset.h"


#define SIG_MAP \
    MAP(SIGHUP, Qnx::QSIGHUP) \
    MAP(SIGINT, Qnx::QSIGINT) \
    MAP(SIGQUIT, Qnx::QSIGQUIT) \
    MAP(SIGILL, Qnx::QSIGILL) \
    MAP(SIGTRAP, Qnx::QSIGTRAP) \
    MAP(SIGIOT, Qnx::QSIGIOT) \
    MAP(SIGFPE, Qnx::QSIGFPE) \
    MAP(SIGKILL, Qnx::QSIGKILL) \
    MAP(SIGBUS, Qnx::QSIGBUS) \
    MAP(SIGSEGV, Qnx::QSIGSEGV) \
    MAP(SIGSYS, Qnx::QSIGSYS) \
    MAP(SIGPIPE, Qnx::QSIGPIPE) \
    MAP(SIGALRM, Qnx::QSIGALRM) \
    MAP(SIGTERM, Qnx::QSIGTERM) \
    MAP(SIGUSR1, Qnx::QSIGUSR1) \
    MAP(SIGUSR2, Qnx::QSIGUSR2) \
    MAP(SIGCHLD, Qnx::QSIGCHLD) \
    MAP(SIGPWR, Qnx::QSIGPWR) \
    MAP(SIGWINCH, Qnx::QSIGWINCH) \
    MAP(SIGURG, Qnx::QSIGURG) \
    MAP(SIGIO, Qnx::QSIGIO) \
    MAP(SIGSTOP, Qnx::QSIGSTOP) \
    MAP(SIGCONT, Qnx::QSIGCONT) \
    MAP(SIGTTIN, Qnx::QSIGTTIN) \
    MAP(SIGTTOU, Qnx::QSIGTTOU)

// handle sigtstp, dev and emt separately

int QnxSigset::map_sig_host_to_qnx(int sig) {
    #define MAP(h, q) case h: return q;
    switch(sig) {
        SIG_MAP
        default:
            return -1;
    }
    #undef MAP
}

int QnxSigset::map_sig_qnx_to_host(int sig) {
    #define MAP(h, q) case q: return h;
    switch(sig) {
        SIG_MAP
        default:
            return -1;
    }
    #undef MAP
}

sigset_t QnxSigset::map_to_host_sigset() const {
    sigset_t acc;
    sigemptyset(&acc);
    for (int sig = Qnx::QSIGMIN; sig <= Qnx::QSIGMAX; sig++) {
        int host_sig = map_sig_qnx_to_host(sig);
        if (host_sig < 0)
            continue;
        if (m_value & get_qnx_mask(sig))
            sigaddset(&acc, host_sig);
    }
    return acc;
}

QnxSigset QnxSigset::map_sigmask_host_to_qnx(const sigset_t &host_set) {
    uint32_t acc = 0;
    for (int sig = Qnx::QSIGMIN; sig <= Qnx::QSIGMAX; sig++) {
        int host_sig = map_sig_qnx_to_host(sig);
        if (host_sig < 0)
            continue;
        if (sigismember(&host_set, host_sig))
            acc |= get_qnx_mask(sig);
    }
    return QnxSigset(acc);
}

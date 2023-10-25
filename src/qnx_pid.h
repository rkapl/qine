#pragma once

#include <unistd.h>
#include <map>
#include <unordered_map>
#include <stdexcept>
#include "cpp.h"
#include "qnx/types.h"

class PidMapFull: std::exception {
};

class QnxPid {
    friend class PidMap;
public:
    static constexpr Qnx::mpid_t PID_PROC = 1;
    static constexpr Qnx::mpid_t PID_PARENT = 2;
    static constexpr Qnx::mpid_t PID_UNKNOWN = 3;
    static constexpr Qnx::mpid_t PID_SELF = 10;

    enum Type {
        PERMANENT, CHILD,
    };

    pid_t host_pid() const { return m_host_pid; }
    Qnx::mpid_t qnx_pid() const { return m_qnx_pid; }
private:
    Type m_type;
    Qnx::mpid_t m_qnx_pid;
    // May be -1 if there is no associated host PID
    pid_t m_host_pid;
};

/* 
 * We need to remap PIDs, since Linux usually does not limit PIDs to 16 bits.
 * We create entries for forked processes, so that we can wait for them, otherwise we ignore them.
 */
class PidMap {
public:
    explicit PidMap();
    ~PidMap();
    QnxPid* alloc_permanent_pid(Qnx::mpid_t pid, int host_pid);
    QnxPid* alloc_pid(int host_pid);

    QnxPid *qnx(Qnx::mpid_t pid);
    QnxPid *host(int pid);
    void free_pid(QnxPid *pid);
private:
    // cannot be copied, because m_reverse_map keeps pointers into m_qnx_map
    NoCopy m_no_copy_marker;

    Qnx::mpid_t m_start_pid;
    Qnx::mpid_t m_last_pid;
    Qnx::mpid_t m_next_pid;

    // we expect PIDs to be very sparse
    std::map<Qnx::mpid_t, QnxPid> m_qnx_map;
    std::unordered_map<int, QnxPid*> m_reverse_map;

};
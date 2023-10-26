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
    static constexpr Qnx::mpid_t PID_UNKNOWN = 2;
    static constexpr Qnx::mpid_t PID_ROOT_PARENT = 3;


    enum Type {
        // not yet initialized, temporary value
        EMPTY,
        // proc, unknown and other special value
        PERMANENT, 
        // parent of our exec'ed process
        ROOT_PARENT, 
        // our current process or one of our fork predecessors
        SELF,
        // our children
        CHILD, 
        // for inherited PIDs that do not any of our PIDs
        SID, PGID,
    };

    pid_t host_pid() const { return m_host_pid; }
    Qnx::mpid_t qnx_pid() const { return m_qnx_pid; }
    Type type() const { return m_type; }
private:
    Type m_type;
    Qnx::mpid_t m_qnx_pid;
    // May be -1 if there is no associated host PID
    pid_t m_host_pid;
};

/* 
 * We need to remap PIDs, since Linux usually does not limit PIDs to 16 bits.
 * We create entries for forked processes, so that we can wait for them.
 *
 * This is a bit complicated by process group IDs and session IDs, which share the same namespace.
 * For now, we assume that the only interesting session IDs and group IDs could be the our one,
 * or the one of our parent. If it changes later, we will get PID_UNKNOWN, that happens.
 */
class PidMap {
public:
    explicit PidMap();
    ~PidMap();
    QnxPid* alloc_permanent_pid(Qnx::mpid_t pid, int host_pid);
    // Allocate pid entry for SELF, SIG or PGID, unless it matches an already known one
    // Accepts host_pid = -1, meaning ignore call, for error handling convenience
    QnxPid* alloc_related_pid(int host_pid, QnxPid::Type type);
    QnxPid* alloc_child_pid(int host_pid);

    QnxPid *qnx(Qnx::mpid_t pid);
    QnxPid *host(int pid);
    void free_pid(QnxPid *pid);
private:
    QnxPid *alloc_empty();
    // cannot be copied, because m_reverse_map keeps pointers into m_qnx_map
    NoCopy m_no_copy_marker;

    Qnx::mpid_t m_start_pid;
    Qnx::mpid_t m_last_pid;
    Qnx::mpid_t m_next_pid;

    // we expect PIDs to be very sparse
    std::map<Qnx::mpid_t, QnxPid> m_qnx_map;
    std::unordered_map<int, QnxPid*> m_reverse_map;

};
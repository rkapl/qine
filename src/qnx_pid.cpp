#include "qnx_pid.h"
#include <time.h>
#include <assert.h>
#include <limits>

PidMap::PidMap()
    :m_start_pid(10), m_last_pid(std::numeric_limits<int16_t>::max())
{

}
PidMap::~PidMap() {}

QnxPid* PidMap::alloc_permanent_pid(Qnx::mpid_t pid, int host_pid)
{
    assert(m_qnx_map.find(pid) == m_qnx_map.end());
    if (host_pid >= 0) {
        assert(m_reverse_map.find(host_pid) == m_reverse_map.end());
    }
    auto pi = &m_qnx_map[pid];
    pi->m_type = QnxPid::PERMANENT;
    pi->m_qnx_pid = pid;
    pi->m_host_pid = host_pid;
    if (host_pid >= 0) {
        m_reverse_map[host_pid] = pi;
    }
    return pi;
}

QnxPid* PidMap::alloc_related_pid(int host_pid, QnxPid::Type type)
{
    assert(type == QnxPid::PGID || type == QnxPid::SID || type == QnxPid::SELF || type == QnxPid::ROOT_PARENT);
    if (host_pid == -1) {
        return nullptr;
    }

    auto pid_iter = m_reverse_map.find(host_pid);
    if (pid_iter != m_reverse_map.end()) {
        return pid_iter->second;
    }

    auto pid_info = alloc_empty(host_pid);
    pid_info->m_type = type;
    pid_info->m_host_pid = host_pid;

    m_reverse_map[pid_info->m_host_pid] = pid_info;
    return pid_info;
}

QnxPid *PidMap::alloc_empty(int host_pid) {
    auto try_pid = compress_pid(host_pid);
    auto it = m_qnx_map.find(try_pid);

    if (it != m_qnx_map.end()) {
        // collision do linear search from semi-random point between start and last
        try_pid = randomize_pid(host_pid);
        it = m_qnx_map.find(try_pid);
        auto start_pid = try_pid;

        // stop iterating if we either cannot find the PID at all or we detect a gap 
        while (it != m_qnx_map.end() && it->first == try_pid) {
            // try next PID
            if (try_pid == m_last_pid) {
                // wrap around
                try_pid = m_start_pid;
                it = m_qnx_map.begin();
            } else {
                // continue search
                try_pid++;
                ++it;
            }
            // are we trying the start pid again?
            if (try_pid == start_pid) {
                throw PidMapFull();
            }
        }
    }

    // create the pid entry
    auto pi = &m_qnx_map[try_pid];
    pi->m_qnx_pid = try_pid;
    pi->m_type = QnxPid::EMPTY;

    return pi;
}


QnxPid* PidMap::alloc_child_pid(int host_pid)
{
    if (host_pid >= 0) {
        assert(m_reverse_map.find(host_pid) == m_reverse_map.end());
    }
    auto pi = alloc_empty(host_pid);
    pi->m_type = QnxPid::CHILD;
    pi->m_host_pid = host_pid;
    if (host_pid >= 0) {
        m_reverse_map[host_pid] = pi;
    }

    return pi;
}

QnxPid *PidMap::qnx(Qnx::mpid_t pid) {
    auto pi = m_qnx_map.find(pid);
    if (pi == m_qnx_map.end())
        return nullptr;
    return &pi->second;
}

QnxPid *PidMap::qnx_valid_host(Qnx::mpid_t pid) {
    auto pi = qnx(pid);
    if (pi->host_pid() == -1) {
        return nullptr;
    }
    return pi;
}

QnxPid *PidMap::host(int pid) {
    auto pi = m_reverse_map.find(pid);
    if (pi == m_reverse_map.end())
        return nullptr;
    return pi->second;
}

void PidMap::free_pid(QnxPid *pid) {
    if (pid->m_host_pid > 0 && pid->m_type == QnxPid::CHILD) {
        m_reverse_map.erase(pid->host_pid());
    }
    m_qnx_map.erase(pid->qnx_pid());
}

uint16_t PidMap::randomize_pid(int host_pid)
{
    uint32_t pid_range = m_last_pid - m_start_pid + 1;
    uint32_t hash = m_start_pid + static_cast<uint32_t>(host_pid) * 3779;
    return m_start_pid + hash % pid_range;
}

uint16_t PidMap::compress_pid(int host_pid)
{
    int pid_range = m_last_pid - m_start_pid + 1;
    // boundary condition for small PIDs, do something reasonable
    if (host_pid < m_start_pid) {
        host_pid += pid_range;
    }
    // preserves small PIDs
    return m_start_pid + (host_pid - m_start_pid) % pid_range;
}
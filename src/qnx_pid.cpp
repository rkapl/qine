#include "qnx_pid.h"
#include <assert.h>
#include <limits>

PidMap::PidMap()
    :m_start_pid(100), m_next_pid(100), m_last_pid(std::numeric_limits<int16_t>::max())
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

QnxPid* PidMap::alloc_pid(int host_pid)
{
    if (host_pid >= 0) {
        assert(m_reverse_map.find(host_pid) == m_reverse_map.end());
    }

    auto try_pid = m_next_pid;
    auto start_pid = try_pid;
    auto it = m_qnx_map.find(try_pid);

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

    // create the pid entry
    auto pi = &m_qnx_map[try_pid];
    pi->m_type = QnxPid::PERMANENT;
    pi->m_qnx_pid = try_pid;
    pi->m_host_pid = host_pid;
    if (host_pid >= 0) {
        m_reverse_map[host_pid] = pi;
    }

    if (try_pid == m_last_pid)
        m_next_pid = m_start_pid;
    else
        m_next_pid = try_pid + 1;
    return pi;
}

QnxPid *PidMap::qnx(Qnx::mpid_t pid) {
    auto pi = m_qnx_map.find(pid);
    if (pi == m_qnx_map.end())
        return nullptr;
    return &pi->second;
}

QnxPid *PidMap::host(int pid) {
    auto pi = m_reverse_map.find(pid);
    if (pi == m_reverse_map.end())
        return nullptr;
    return pi->second;
}

void PidMap::free_pid(QnxPid *pid) {
    if (pid->m_host_pid && pid->m_type != QnxPid::PERMANENT)
        m_reverse_map.erase(pid->host_pid());
    m_qnx_map.erase(pid->qnx_pid());
}
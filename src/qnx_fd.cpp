#include "qnx_fd.h"
#include "log.h"
#include <stdexcept>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <unordered_map>

FdMap::FdMap(): m_fds(1024*16) {
}

FdMap::~FdMap() {

}

void FdMap::scan_host_fds(Qnx::nid_t nid, Qnx::pid_t pid, Qnx::pid_t vid) {
    DIR *fd_dir = opendir("/proc/self/fd");
    if (!fd_dir) {
        perror("scan /proc/self/fd");
        throw std::system_error(errno, std::system_category());
    }

    int scan_fd = dirfd(fd_dir);
    dirent *fd_entry;
    while ((fd_entry = readdir(fd_dir))) {
        if (fd_entry->d_name[0] == '.')
            continue;
        int fd = atoi(fd_entry->d_name);
        if (fd == scan_fd)
            continue;
        auto fdi = m_fds.alloc_exactly_at(fd, [=] (int fd) { return new QnxFd(fd, nid, pid, vid, 0);});
        fdi->m_open = true;
        fdi->m_host_fd = fdi->m_fd;
    }

    closedir(fd_dir);
}

QnxFd *FdMap::fd_query(Qnx::fd_t start) {
    return m_fds[m_fds.search(start, true)];
}

// Corresponds to qnx_fd_attach with owner_pid zero. Throws NoFreeId
QnxFd* FdMap::qnx_fd_attach(Qnx::fd_t first_fd, Qnx::nid_t nid, Qnx::mpid_t pid,
                    Qnx::mpid_t vid, uint16_t flags)
{
    return m_fds.alloc_starting_at(first_fd, [=](int fd) {return new QnxFd(fd, nid, pid, vid, flags);});
}

// Detaches FD, returns false if fd was not attached
bool FdMap::qnx_fd_detach(Qnx::fd_t fd)
{
    bool used = bool(m_fds[fd]);
    m_fds.free(fd);
    return used;
}

bool FdMap::assign_fds(size_t count, QnxFd** to_fds, int *host_fds) {
    // first store where the host FDs live
    std::unordered_map<int, int> fd_to_index;
    for (size_t i = 0; i < count; i++) {
        fd_to_index[host_fds[i]] = i;
        Log::print(Log::FD, "opened fd %d using host fd %d\n", to_fds[i]->m_fd,  host_fds[i]);
    }

    auto host_fd_moved = [&](int index, int to) {
        int from = host_fds[index];
        host_fds[index] = to;
        Log::print(Log::FD, "moved host fd %d -> %d\n", from, to);
        fd_to_index.erase(from);
        ::close(from);
        fd_to_index[to] = index;
    };

    // Move the host_fd anywhere else (if it is opened by the host) and make sure host_fd is free
    auto realloc_fd = [&](int host_fd) -> bool {
        auto it = fd_to_index.find(host_fd);
        if (it != fd_to_index.end()) {
            int r = dup(host_fd);
            if (r < 0)
                return false;
            host_fd_moved(it->first, r);
        }
        return true;
    };

    // algorithm:
    // We go one-by one and try to fix the FD numbers.
    // If the FD number is already taken, we dup it somewhere else temporarily.
    for (size_t i = 0; i < count; i++) {
        auto fd = to_fds[i];
        if (fd->m_fd == host_fds[i]) {
            fd->m_host_fd = fd->m_fd;
            fd->m_open = true;
            continue;
        }

        if (!realloc_fd(fd->m_fd))
            return false;

        int r = dup2(host_fds[i], fd->m_fd);
        if (r < 0)
            return false;

        host_fd_moved(i, fd->m_fd);
        fd->m_host_fd = fd->m_fd;
        fd->m_open = true;
    }
    return true;
}

QnxFd::QnxFd(Qnx::fd_t fd, Qnx::nid_t nid, Qnx::mpid_t pid, Qnx::mpid_t vid, uint16_t flags)
    :m_fd(fd), m_nid(nid), m_pid(pid), m_vid(vid), m_flags(flags),
    m_handle(0), m_open(false), m_host_fd(0), m_host_dir(NULL)

{}

QnxFd::~QnxFd() {
    if (m_open) {
        close();
    }
}

bool QnxFd::is_close_on_exec() {
    return m_flags & 0x40u;
}

bool QnxFd::assign_fd(UniqueFd&& tmp_fd) {
    // TODO: handle CLOEXEC here? Now it is a mess. And we need it for dup, open, pipe etc.
    assert(!m_open);
    if (tmp_fd.get() != m_fd) {
        Log::print(Log::FD, "opened fd %d using host fd %d\n", m_fd,  tmp_fd.get());
        // move fd to final location
        UniqueFd r(dup2(tmp_fd.get(), m_fd));
        if (!r.valid()) {
            Log::print(Log::FD, "failed to remap fd: %s\n", strerror(errno));
            return false;
        } else {
            m_host_fd = r.release();
            m_open = true;
            return true;
        }
    } else {
        Log::print(Log::FD, "opened fd %d\n", m_fd);
        m_host_fd = tmp_fd.release();
        m_open = true;
        return true;
    }
}

bool QnxFd::close() {
    assert(m_open);
    int r;
    if (m_host_dir) {
        r = closedir(m_host_dir);
    } else {
        r = ::close(m_host_fd);
    }
    m_open = false;
    m_host_dir = NULL;
    m_host_fd = -1;
    return r >= 0;
}

void QnxFd::check_open() {
    if (!m_open)
        throw BadFdException();
}

bool QnxFd::prepare_dir() {
    if (!m_host_dir) {
        m_host_dir = fdopendir(m_host_fd);
        return m_host_dir != nullptr;
    }
    return true;
}
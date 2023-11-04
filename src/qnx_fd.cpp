#include "qnx_fd.h"
#include "log.h"
#include "unique_fd.h"
#include <fcntl.h>
#include <stdexcept>
#include <iostream>
#include <string.h>
#include <system_error>
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
        Log::print(Log::FD, "inherited fd %d\n", fd);
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

bool FdMap::assign_fds(size_t count, QnxFd** to_fds, UniqueFd *host_fds) {
    // first store where the host FDs live
    std::unordered_map<int, int> fd_to_index;
    for (size_t i = 0; i < count; i++) {
        fd_to_index[host_fds[i].get()] = i;
        Log::print(Log::FD, "assigned fd %d using host fd %d\n", to_fds[i]->m_fd,  host_fds[i].get());
    }

    auto host_fd_moved = [&](int index, UniqueFd&& to) {
        int from = host_fds[index].get();
        host_fds[index] = std::move(to);
        Log::print(Log::FD, "moved host fd %d -> %d\n", from, to.get());
        fd_to_index.erase(from);
        fd_to_index[to.get()] = index;
    };

    // Move the host_fd anywhere else (if it is opened by the host) and make sure host_fd is free (can be overwritten)
    auto realloc_fd = [&](int host_fd) -> bool {
        auto it = fd_to_index.find(host_fd);
        if (it != fd_to_index.end()) {
            UniqueFd new_fd = dup(host_fd);
            if (!new_fd.valid())
                return false;
            host_fd_moved(it->first, std::move(new_fd));
        }
        Log::print(Log::FD, "host FD %d free, no need to move existing FD\n", host_fd);
        return true;
    };

    // algorithm:
    // We go one-by one and try to fix the FD numbers.
    // If the FD number is already taken, we dup it somewhere else temporarily.
    for (size_t i = 0; i < count; i++) {
        auto fd = to_fds[i];
        if (fd->m_fd == host_fds[i].get()) {
            Log::print(Log::FD, "FD %d matches, moving on\n", fd->m_fd);
            // move the ownership to our FD object
            fd_to_index.erase(fd->m_host_fd);
            continue;
        }

        if (!realloc_fd(fd->m_fd))
            return false;

        UniqueFd new_fd(dup2(host_fds[i].get(), fd->m_fd));
        if (!new_fd.valid())
            return false;

        host_fd_moved(i, std::move(new_fd));
    }

    // now move the ownership and finalize
    for (size_t i = 0; i < count; i++) {
        auto fd = to_fds[i];
        fd->m_open = true;
        fd->m_host_fd = host_fds[i].release();
        if (fd->is_close_on_exec()) {
            int r = fcntl(fd->m_host_fd, F_SETFD, FD_CLOEXEC);
            if (r < 0)
                throw std::system_error(errno, std::system_category());
        }
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
    assert(!m_open);
    UniqueFd final_fd;
    if (tmp_fd.get() != m_fd) {
        Log::print(Log::FD, "assigned fd %d using host fd %d\n", m_fd,  tmp_fd.get());
        // move fd to final location
        final_fd = UniqueFd(dup2(tmp_fd.get(), m_fd));
        if (!final_fd.valid()) {
            Log::print(Log::FD, "failed to remap fd: %s\n", strerror(errno));
            return false;
        }
    } else {
        Log::print(Log::FD, "assigned fd %d\n", m_fd);
        final_fd = std::move(tmp_fd);
    }

    m_host_fd = final_fd.release();
    m_open = true;
    if (is_close_on_exec()) {
        int r = fcntl(m_host_fd, F_SETFD, FD_CLOEXEC);
        if (r < 0)
            throw std::system_error(errno, std::system_category());
    }
    return true;
}

bool QnxFd::close() {
    assert(m_open);
    Log::print(Log::FD, "fd %d close\n", m_fd);
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
    if (!m_open) {
        Log::print(Log::FD, "fd %d not open\n", m_fd);
        throw BadFdException();
    }
}

bool QnxFd::prepare_dir() {
    if (!m_host_dir) {
        m_host_dir = fdopendir(m_host_fd);
        return m_host_dir != nullptr;
    }
    return true;
}
#include "qnx_fd.h"
#include <stdexcept>
#include <iostream>
#include <unistd.h>

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

void QnxFd::mark_open() {
    assert(!m_open);
    m_open = true;
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
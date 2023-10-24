#pragma once

#include "qnx/types.h"
#include "idmap.h"
#include "path_mapper.h"
#include <dirent.h>
#include <stdexcept>
#include <string>

class QnxFd;

class BadFdException : public std::exception {};

class FdMap {
  public:
    FdMap();
    ~FdMap();
    void scan_host_fds(Qnx::nid_t nid, Qnx::pid_t pid, Qnx::pid_t vid);
    
    // Corresponds to qnx_fd_attach with owner_pid zero. Throws NoFreeId
    QnxFd* qnx_fd_attach(Qnx::fd_t first_fd, Qnx::nid_t nid, Qnx::mpid_t pid,
                       Qnx::mpid_t vid, uint16_t flags);
    // Detaches FD, returns false if fd was not attached
    bool qnx_fd_detach(Qnx::fd_t fd);

    /* Get an FD, checking that it is open with host FD, otherwise throw badfd
     */
    inline QnxFd *get_open_fd(Qnx::fd_t fdi);
    inline int get_host_fd(Qnx::fd_t fdi) { return fdi; }
    /* Get an FD, otherwise throw badfd */
    inline QnxFd *get_attached_fd(Qnx::fd_t fdi);
    /* Finds a query higher or equal than start */
    QnxFd *fd_query(Qnx::fd_t start);

    // The rest of the function exist on QnxFd
  private:
    IdMap<QnxFd> m_fds;
};

/* Attached FD. Need not be opened FD. */
class QnxFd {
  public:
    QnxFd(Qnx::fd_t fd, Qnx::nid_t nid, Qnx::mpid_t pid, Qnx::mpid_t vid,
          uint16_t flags);
    ~QnxFd();

    void mark_open();
    bool close();
    void check_open();

    // error in errno if false
    bool prepare_dir();

    bool is_close_on_exec();

    /* QNX information, we do not use all that, be we store it during attach*/
    int m_fd;

    Qnx::nid_t m_nid;
    Qnx::mpid_t m_pid;
    Qnx::mpid_t m_vid;
    uint16_t m_flags;

    uint32_t m_handle;

    /* Host information*/
    bool m_open;
    // path can be empty for inherited FDs, use resolve_path
    PathInfo m_path;
    // same as m_fd
    int m_host_fd;
    // if readdir was used, we allocate DIR*
    DIR *m_host_dir;
};

QnxFd *FdMap::get_open_fd(Qnx::fd_t fdi) {
    auto fd = get_attached_fd(fdi);
    fd->check_open();
    return fd;
}

QnxFd *FdMap::get_attached_fd(Qnx::fd_t fdi) {
    auto fd = m_fds[fdi];
    if (!fd)
        throw BadFdException();
    return fd;
}
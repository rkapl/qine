#pragma once

#include "idmap.h"
#include "path_mapper.h"
#include "qnx/types.h"
#include "unique_fd.h"
#include "log.h"
#include <dirent.h>
#include <stdexcept>
#include <string>

class QnxFd;
class FdFilter;

class BadFdException : public std::exception {};

class FdMap {
  public:
    FdMap();
    ~FdMap();

    // Create entries for existing host FDs at startup
    void scan_host_fds(Qnx::nid_t nid, Qnx::pid_t pid, Qnx::pid_t vid);

    // Corresponds to qnx_fd_attach with owner_pid zero. Throws NoFreeId
    QnxFd *qnx_fd_attach(Qnx::fd_t first_fd, Qnx::nid_t nid, Qnx::mpid_t pid,
                         Qnx::mpid_t vid, uint16_t flags);
    // Detaches FD, returns false if fd was not attached
    bool qnx_fd_detach(Qnx::fd_t fd);

    /* Get an FD, checking that it is open with host FD, otherwise throw badfd
     */
    inline QnxFd *get_open_fd(Qnx::fd_t fdi);
    /* get_open_fd() -> host_fd*/
    inline int get_host_fd(Qnx::fd_t fdi) { return fdi; }
    /* Get an FD, otherwise throw badfd */
    inline QnxFd *get_attached_fd(Qnx::fd_t fdi);
    /* Finds a query higher or equal than start */
    QnxFd *fd_query(Qnx::fd_t start);

    /* Used when creating multiple FDs. Takes N arbitrrary, already open host
     * FDs and changes their FD numbers to match the QNX to_fd FDs and fills in
     * the host FD numbers in the descriptors.
     *
     * errno if false.
     * Moves out from host_fds, atomically. But the FD may be renumbered on
     * failure (not atomic).
     */
    bool assign_fds(size_t count, QnxFd **to_fds, UniqueFd *host_fds);

    // The rest of the function exist on QnxFd
  private:
    IdMap<QnxFd> m_fds;
};

/* Attached FD. Need not be opened FD. Currently we alwayas have a backing host FD*/
class QnxFd {
  public:
    QnxFd(Qnx::fd_t fd, Qnx::nid_t nid, Qnx::mpid_t pid, Qnx::mpid_t vid,
          uint16_t flags);
    ~QnxFd();

    // Assign a host fd (after remaping) and mark open.
    // errno if false
    bool assign_fd(UniqueFd &&host_fd);
    bool close();
    void check_open();

    // error in errno if false
    bool prepare_dir();

    bool is_close_on_exec();

    /* QNX information, we do not use all that, be we store it during attach */
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
    std::unique_ptr<FdFilter> m_filter;
};

QnxFd *FdMap::get_open_fd(Qnx::fd_t fdi) {
    auto fd = get_attached_fd(fdi);
    fd->check_open();
    return fd;
}

QnxFd *FdMap::get_attached_fd(Qnx::fd_t fdi) {
    auto fd = m_fds[fdi];
    if (!fd) {
        Log::print(Log::FD, "fd %d not attached\n", fdi);
        throw BadFdException();
    }
    return fd;
}
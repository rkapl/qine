#pragma  once

#include "emu.h"
#include "msg.h"
#include "msg_handler.h"
#include "qnx/types.h"
#include <csignal>
#include <cstdint>
#include <sys/stat.h>

namespace QnxMsg::io {
    struct stat;
};

class InconsistentFd: std::runtime_error {
public:
    InconsistentFd(): std::runtime_error("cannot obtain information for opened FD") {}
};

/* Handles proc messages and passtrough FD messages */
class MainHandler: public MsgHandler {
public:
    void receive(MsgInfo& msg);
private:
    std::string get_fd_path(int fd);
    uint32_t map_file_flags_to_host(uint32_t flags);

    void receive_inner(MsgInfo &i);

    void proc_segment_realloc(MsgInfo &i);
    void proc_segment_alloc(MsgInfo &i);
    void proc_time(MsgInfo &i);
    void proc_open(MsgInfo &i);
    void proc_terminate(MsgInfo &i);
    void proc_fd_attach(MsgInfo &i);
    void proc_fd_detach(MsgInfo &i);
    void proc_fd_query(MsgInfo &i);
    void proc_fd_action1(MsgInfo &i);
    void proc_vc_detach(MsgInfo &i);
    void proc_vc_attach(MsgInfo &i);
    void proc_psinfo(MsgInfo &i);
    void proc_sigtab(MsgInfo &i);
    void proc_sigact(MsgInfo &i);
    void proc_getid(MsgInfo &i);
    void proc_sigmask(MsgInfo &i);
    void proc_osinfo(MsgInfo &i);
    void proc_prefix(MsgInfo &i);

    void proc_fork(MsgInfo &i);
    void proc_spawn(MsgInfo &i);
    void proc_exec(MsgInfo &i);
    void proc_exec_common(MsgInfo &i);
    void proc_wait(MsgInfo &i);

    void io_open(MsgInfo &i);
    void io_chdir(MsgInfo &i);
    void io_stat(MsgInfo &i);
    void io_rename(MsgInfo &i);
    void io_fstat(MsgInfo &i);
    void io_close(MsgInfo &i);
    void io_read(MsgInfo &i);
    void io_write(MsgInfo &i);
    void io_lseek(MsgInfo &i);
    void io_readdir(MsgInfo &i);
    void io_rewinddir(MsgInfo &i);
    void io_fcntl_flags(MsgInfo &i);
    void io_dup(MsgInfo &i);
    void io_fpathconf(MsgInfo &i);
    void io_chmod(MsgInfo &i);
    void io_chown(MsgInfo &i);
    void io_utime(MsgInfo &i);

    void fsys_unlink(MsgInfo &i);
    void fsys_mkspecial(MsgInfo &i);
    void fsys_link(MsgInfo &i);
    void fsys_sync(MsgInfo &i);
    void fsys_readlink(MsgInfo &i);
    void fsys_trunc(MsgInfo &i);
    void fsys_fsync(MsgInfo &i);

    void transfer_stat(QnxMsg::io::stat& dst, struct stat& src);

    void dev_tcgetattr(MsgInfo &i);
    void dev_tcsetattr(MsgInfo &i);
    void dev_term_size(MsgInfo &i);
};
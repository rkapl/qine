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
    void receive(MsgContext& msg);
private:
    std::string get_fd_path(int fd);
    uint32_t map_file_flags_to_host(uint32_t flags);

    void receive_inner(MsgContext &i);

    void proc_segment_realloc(MsgContext &i);
    void proc_segment_alloc(MsgContext &i);
    void proc_time(MsgContext &i);
    void proc_open(MsgContext &i);
    void proc_terminate(MsgContext &i);
    void proc_fd_attach(MsgContext &i);
    void proc_fd_detach(MsgContext &i);
    void proc_fd_query(MsgContext &i);
    void proc_fd_action1(MsgContext &i);
    void proc_vc_detach(MsgContext &i);
    void proc_vc_attach(MsgContext &i);
    void proc_psinfo(MsgContext &i);
    void proc_sigtab(MsgContext &i);
    void proc_sigact(MsgContext &i);
    void proc_getid(MsgContext &i);
    void proc_sigmask(MsgContext &i);
    void proc_osinfo(MsgContext &i);
    void proc_prefix(MsgContext &i);
    void proc_sid_query(MsgContext &i);
    void proc_sid_set(MsgContext &i);

    void proc_fork(MsgContext &i);
    void proc_spawn(MsgContext &i);
    void proc_exec(MsgContext &i);
    void proc_exec_common(MsgContext &i);
    void proc_timer_create(MsgContext &i);
    void proc_timer_settime(MsgContext &i);
    void proc_wait(MsgContext &i);

    void io_open(MsgContext &i);
    void io_chdir(MsgContext &i);
    void io_stat(MsgContext &i);
    void io_rename(MsgContext &i);
    void io_fstat(MsgContext &i);
    void io_close(MsgContext &i);
    void io_read(MsgContext &i);
    void io_write(MsgContext &i);
    void io_lseek(MsgContext &i);
    void io_readdir(MsgContext &i);
    void io_rewinddir(MsgContext &i);
    void io_fcntl_flags(MsgContext &i);
    void io_dup(MsgContext &i);
    void io_fpathconf(MsgContext &i);
    void io_chmod(MsgContext &i);
    void io_chown(MsgContext &i);
    void io_utime(MsgContext &i);

    void fsys_unlink(MsgContext &i);
    void fsys_mkspecial(MsgContext &i);
    void fsys_link(MsgContext &i);
    void fsys_sync(MsgContext &i);
    void fsys_readlink(MsgContext &i);
    void fsys_trunc(MsgContext &i);
    void fsys_fsync(MsgContext &i);

    void transfer_stat(QnxMsg::io::stat& dst, struct stat& src);

    void dev_tcgetattr(MsgContext &i);
    void dev_tcsetattr(MsgContext &i);
    void dev_term_size(MsgContext &i);
};
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

namespace QnxMsg::dev {
    struct dev_info;
}


class QnxFd;

/* Handles proc messages and passtrough FD messages */
class MainHandler: public MsgHandler {
public:
    void receive(MsgContext& msg);
private:
    std::string get_fd_path(int fd);
    uint32_t map_file_flags_to_host(uint32_t flags);

    void receive_inner(MsgContext &i);

    void proc_slib_register(MsgContext &i);
    void proc_segment_realloc(MsgContext &i);
    void proc_segment_alloc(MsgContext &i);
    void proc_segment_put(MsgContext &i);
    void proc_segment_arm(MsgContext &i);
    void proc_segment_priv(MsgContext &i);
    void proc_name_attach(MsgContext &i);
    void proc_time(MsgContext &i);
    void proc_setpgid(MsgContext &i);
    void proc_open(MsgContext &i);
    void proc_terminate(MsgContext &i);
    void proc_fd_attach(MsgContext &i);
    void proc_fd_detach(MsgContext &i);
    void proc_fd_query(MsgContext &i);
    void proc_fd_setfd(MsgContext &i);
    void proc_vc_detach(MsgContext &i);
    void proc_vc_attach(MsgContext &i);
    void proc_psinfo(MsgContext &i);
    void proc_sigtab(MsgContext &i);
    void proc_sigact(MsgContext &i);
    void proc_sigraise(MsgContext &i);
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
    void proc_timer_alarm(MsgContext &i);
    void proc_wait(MsgContext &i);

    void proc_sem_init(MsgContext &i);
    void proc_sem_destroy(MsgContext &i);

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
    void io_ioctl(MsgContext &i);
    void io_qioctl(MsgContext &i);
    void io_lock(MsgContext &i);

    void fsys_unlink(MsgContext &i);
    void fsys_mkspecial(MsgContext &i);
    void fsys_link(MsgContext &i);
    void fsys_sync(MsgContext &i);
    void fsys_readlink(MsgContext &i);
    void fsys_trunc(MsgContext &i);
    void fsys_fsync(MsgContext &i);
    void fsys_pipe(MsgContext &i);
    void fsys_disk_entry(MsgContext &i);
    void fsys_get_mount(MsgContext &i);
    void fsys_disk_space(MsgContext &i);

    void transfer_stat(QnxMsg::io::stat& dst, struct stat& src);

    void dev_fdinfo(MsgContext &i);
    void dev_info(MsgContext &i);
    // errno if false
    bool fill_dev_info(MsgContext &i, QnxFd *fd, QnxMsg::dev::dev_info *dst);

    // term_handler.cpp
    void terminal_qioctl(Ioctl &i);

    void dev_tcgetattr(MsgContext &i);
    void dev_tcsetattr(MsgContext &i);
    void dev_tcsetpgrp(MsgContext &i);
    void dev_tcgetpgrp(MsgContext &i);
    void dev_term_size(MsgContext &i);
    void dev_tcdrain(MsgContext &i);
    void dev_tcflush(MsgContext &i);
    void dev_read(MsgContext &i);
    void dev_insert_chars(MsgContext &i);
    void dev_mode(MsgContext &i);
    void ioctl_terminal_get_size(Ioctl &i);
    void ioctl_terminal_set_size(Ioctl &i);

    // These low-level handler return QNX errno and translate QNX calls to host calls
    // They are used both for ioctl-style and msg-style calls, if possible

    uint16_t handle_tcgetattr(MsgContext &i, int16_t qnx_fd, Qnx::termios *dst);
    uint16_t handle_tcsetattr(MsgContext &i, int16_t qnx_fd, const Qnx::termios *termios, int optional_actions);
    uint16_t handle_tcsetpgrp(MsgContext &i, int16_t qnx_fd, int16_t qnx_pgrp);
    uint16_t handle_tcgetpgrp(MsgContext &i, int16_t qnx_fd, int16_t *qnx_pgrp_dst);
};
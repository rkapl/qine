#pragma  once

#include "emu.h"
#include "msg_handler.h"
#include <sys/stat.h>

namespace QnxMsg::io {
    struct stat;
};

/* Handles proc messages and passtrough FD messages */
class MainHandler: public MsgHandler {
public:
    void receive(MsgInfo& msg);
private:
    void msg_handle(MsgInfo &i);

    void proc_segment_realloc(MsgInfo &i);
    void proc_segment_alloc(MsgInfo &i);
    void proc_time(MsgInfo &i);
    void proc_open(MsgInfo &i);
    void proc_terminate(MsgInfo &i);
    void proc_fd_attach(MsgInfo &i);
    void proc_fd_detach(MsgInfo &i);
    void proc_vc_detach(MsgInfo &i);
    void proc_psinfo(MsgInfo &i);
    void proc_sigtab(MsgInfo &i);
    void proc_sigact(MsgInfo &i);
    void proc_getid(MsgInfo &i);
    void proc_sigmask(MsgInfo &i);

    void io_open(MsgInfo &i);
    void io_stat(MsgInfo &i);
    void io_fstat(MsgInfo &i);
    void io_close(MsgInfo &i);
    void io_read(MsgInfo &i);
    void io_write(MsgInfo &i);
    void io_lseek(MsgInfo &i);
    void io_readdir(MsgInfo &i);
    void io_rewinddir(MsgInfo &i);

    void fsys_unlink(MsgInfo &i);

    void transfer_stat(QnxMsg::io::stat& dst, struct stat& src);

    void dev_tcgetattr(MsgInfo &i);
};
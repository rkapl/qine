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
    void proc_time(MsgInfo &i);
    void proc_open(MsgInfo &i);
    void proc_terminate(MsgInfo &i);
    void proc_fd_attach(MsgInfo &i);
    void proc_fd_detach(MsgInfo &i);
    void proc_vc_detach(MsgInfo &i);

    void io_open(MsgInfo &i);
    void io_stat(MsgInfo &i);
    void io_close(MsgInfo &i);
    void io_read(MsgInfo &i);
    void io_write(MsgInfo &i);
    void io_lseek(MsgInfo &i);

    void fsys_unlink(MsgInfo &i);

    void transfer_stat(QnxMsg::io::stat& dst, struct stat& src);

    void dev_tcgetattr(MsgInfo &i);
};
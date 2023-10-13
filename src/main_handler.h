#pragma  once

#include "msg_handler.h"

/* Handles proc messages and passtrough FD messages */
class MainHandler: public MsgHandler {
public:
    void receive(MsgInfo& msg);
private:
    void msg_handle(MsgInfo &i);

    void proc_open(MsgInfo &i);
    void proc_segment_realloc(MsgInfo &i);
    void proc_terminate(MsgInfo &i);
    void proc_fd_attach(MsgInfo &i);
    void proc_fd_detach(MsgInfo &i);

    void io_open(MsgInfo &i);
    void io_close(MsgInfo &i);
    void io_read(MsgInfo &i);
    void io_write(MsgInfo &i);
    void io_lseek(MsgInfo &i);

    void dev_tcgetattr(MsgInfo &i);
};
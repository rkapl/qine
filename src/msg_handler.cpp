#include "msg_handler.h"

#include "process.h"

int MsgContext::map_fd(Qnx::fd_t qnx_fd) {
    return proc().fds().get_host_fd(qnx_fd);
}
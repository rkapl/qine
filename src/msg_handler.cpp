#include "msg_handler.h"

#include "process.h"

Ioctl::Ioctl(MsgContext* ctx)
    :m_ctx(ctx), m_status(Qnx::QENOSYS), m_retval(0)
{
    m_ctx->msg().read_type(&m_header);
}

void Ioctl::write_response_header() {
    QnxMsg::io::qioctl_reply reply;
    reply.m_padd_1 = 0;
    reply.m_status = m_status;
    reply.m_ret_val = m_retval;
    m_ctx->msg().write_type(0, &reply);
}

int MsgContext::map_fd(Qnx::fd_t qnx_fd) {
    return proc().fds().get_host_fd(qnx_fd);
}
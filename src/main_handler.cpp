#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>

#include "mem_ops.h"
#include "process.h"
#include "msg_handler.h"
#include "main_handler.h"
#include "qnx/msg.h"
#include <gen_msg/proc.h>
#include <gen_msg/io.h>


void MainHandler::receive(MsgInfo& i) {
    Qnx::MsgHeader hdr;
    i.msg().read_type(&hdr);

    switch (hdr.type) {
        case QnxMsg::proc::msg_segment_realloc::TYPE :
            switch (hdr.subtype) {
                case QnxMsg::proc::msg_segment_realloc::SUBTYPE:
                    proc_segment_realloc(i);
                    break;
            }
            break;
        case QnxMsg::proc::msg_terminate::TYPE:
            proc_terminate(i);
            break;
        case QnxMsg::proc::msg_open::TYPE:
            proc_open(i);
            break;

        case QnxMsg::io::msg_read::TYPE:
            io_read(i);
            break;
        case QnxMsg::io::msg_write::TYPE:
            io_write(i);
            break;
        case QnxMsg::io::msg_lseek::TYPE:
            io_lseek(i);
            break;
        default:
            printf("Unhandled message dec %d:%d\n", hdr.type, hdr.subtype);
            break;
    }
}

void MainHandler::proc_terminate(MsgInfo& i)
{
    QnxMsg::proc::terminate_request msg;
    i.msg().read_type(&msg);
    Emu::debug_hook();
    exit(msg.m_status);
}

void MainHandler::proc_segment_realloc(MsgInfo& i)
{
    QnxMsg::proc::segment_realloc_request msg;
    i.msg().read_type(&msg);
    // TODO: handle invalid segments
    auto sd  = i.ctx().proc()->descriptor_by_selector(msg.m_sel);
    auto seg = sd->segment();
    GuestPtr base = seg->paged_size();

    QnxMsg::proc::segment_realloc_reply reply;
    memset(&reply, 0, sizeof(reply));
    if (seg->is_shared()) {
        reply.m_status = Qnx::QEBUSY;
    } else {
        if (seg->size() < msg.m_nbytes) {
            seg->grow(Access::READ_WRITE, MemOps::align_page_up(msg.m_nbytes - seg->size()));
            // TODO: this must be done better, the segment must be aware of its descriptors,
            // at least code and data 
            sd->update_descriptors();
        }
        reply.m_status = Qnx::QEOK;
        reply.m_sel = msg.m_sel;
        reply.m_nbytes = seg->size();
        reply.m_addr = reinterpret_cast<uint32_t>(seg->location()); 
        //printf("New segment size: %x\n", seg->size());
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::proc_open(MsgInfo& i) {
    QnxMsg::proc::open msg;
    i.msg().read_type(&msg);
    
    int mapped_oflags = O_RDONLY;
    int fd = ::open(msg.m_path, mapped_oflags);
    msg.m_fd = fd;
    msg.m_pid = 1;
    if (fd < 0) {
        msg.m_type = Emu::map_errno(errno);
    } else {
        msg.m_type = 0;
    }

    i.msg().write_type(0, &msg);
}

void MainHandler::io_lseek(MsgInfo &i) {
    QnxMsg::io::lseek_request msg;
    i.msg().read_type(&msg);

    // STUB

    QnxMsg::io::lseek_reply reply;
    reply.m_status = Qnx::QEOK;
    reply.m_zero = 0;
    reply.m_offset = 0;
    i.msg().write_type(0, &reply);
}

void MainHandler::io_read(MsgInfo &i) {
    QnxMsg::io::read_request msg;
    i.msg().read_type(&msg);

    std::vector<struct iovec> iov;
    i.msg().write_iovec(sizeof(msg), msg.m_nbytes, iov);

    QnxMsg::io::read_reply reply;
    int r = readv(msg.m_fd, iov.data(), iov.size());
    reply.m_zero = 0;
    if (r < 0) {
        reply.m_status = errno;
        reply.m_nbytes = 0;
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_nbytes = r;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::io_write(MsgInfo &i) {
    QnxMsg::io::write_request msg;
    i.msg().read_type(&msg);

    std::vector<struct iovec> iov;
    i.msg().read_iovec(sizeof(msg), msg.m_nbytes, iov);

    QnxMsg::io::write_reply reply;
    int r = writev(msg.m_fd, iov.data(), iov.size());
    reply.m_zero = 0;
    if (r < 0) {
        reply.m_status = errno;
        reply.m_nbytes = 0;
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_nbytes = r;
    }
    i.msg().write_type(0, &reply);
}
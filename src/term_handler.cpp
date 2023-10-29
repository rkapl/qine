#include <termios.h>
#include <sys/ioctl.h>

#include "gen_msg/dev.h"
#include "main_handler.h"
#include "process.h"
#include "msg.h"
#include "msg/meta.h"
#include "log.h"
#include "qnx/term.h"
#include "qnx/ioctl.h"

void MainHandler::terminal_qioctl(Ioctl &i) {
    /* unix.a (ioctl) translates lot of calls to the appropriate messages, but we must also handle the IOCTLs*/

    /* ok, it turns of we should not handle that!
     * some bash 2.00 binary is buggy and calls us with bogus receive address, 
     * probably because someone did #define ioctl qnx_ioctl
     */

    #if 0
    return;

    auto p = [] (int i) {return Qnx::ioctl_param(i); };
    switch (i.request_code()) {
        case p(Qnx::QTCSETS): {
            Qnx::termios termios;
            i.set_retval(handle_tcgetattr(i.ctx(), i.fd(), &termios));
            i.write(&termios);
        }; break;
        case p(Qnx::QTCGETS): {
            Qnx::termios termios; i.read(&termios);
            i.set_retval(handle_tcsetattr(i.ctx(), i.fd(), &termios, 0));
        }; break;
        case p(Qnx::QTIOCGETPGRP): {
            int16_t pgrp;
            i.set_retval(handle_tcgetpgrp(i.ctx(), i.fd(), &pgrp));
            i.write(&pgrp);
        }; break;
        case p(Qnx::QTIOCSETPGRP): {
            i.set_retval(handle_tcsetpgrp(i.ctx(), i.fd(), i.read_value<int16_t>()));
        }; break;
        case p(Qnx::QTIOCGWINSZ):
            ioctl_terminal_get_size(i);
        break;
        case p(Qnx::QTIOCSWINSZ):
            ioctl_terminal_set_size(i);
        break;
        default:
            Log::print(Log::UNHANDLED, "Unhandled terminal IOCTL %d (full %x)\n", i.request_number(), i.request_code());
    }
    #endif
}

uint16_t MainHandler::handle_tcgetattr(MsgContext &i, int16_t qnx_fd, Qnx::termios *qnx_attr) {
    struct termios attr;
    int r = tcgetattr(i.map_fd(qnx_fd), &attr);
    if (r < 0) {
        return Emu::map_errno(errno);
    }

    // TODO: do a real translate
    for(size_t i = 0; i < std::min(NCCS, Qnx::Q_NCCS); i++) {
        qnx_attr->c_cc[i] = attr.c_cc[i];
    }
    qnx_attr->c_ispeed = attr.c_ispeed;
    qnx_attr->c_ospeed = attr.c_ospeed;
    //reply.m_state.c_line = attr.c_line;
    //qnx_attr->c_cflag = attr.c_cflag;
    //qnx_attr->c_iflag = attr.c_iflag;
    //qnx_attr->c_lflag = attr.c_lflag;
    //qnx_attr->c_oflag = attr.c_oflag;
    return Qnx::QEOK;
}

uint16_t MainHandler::handle_tcsetattr(MsgContext &i, int16_t qnx_fd, const Qnx::termios *termios, int optional_actions) {
    struct termios attr;
    // TODO: do a real translate

    for(size_t i = 0; i < std::min(NCCS, Qnx::Q_NCCS); i++) {
        attr.c_cc[i] = termios->c_cc[i];
    }
    attr.c_ispeed = termios->c_ispeed;
    attr.c_ospeed = termios->c_ospeed;
    //attr.c_line = msg.m_state.m_c;
    attr.c_cflag = termios->c_cflag;
    attr.c_iflag = termios->c_iflag;
    attr.c_lflag = termios->c_lflag;
    attr.c_oflag = termios->c_oflag;

    int r = tcsetattr(i.map_fd(i.m_fd), optional_actions, &attr);
    if (r < 0) {
        return Emu::map_errno(errno);
    } else {
        return Qnx::QEOK;
    }   
}

uint16_t MainHandler::handle_tcsetpgrp(MsgContext &i, int16_t qnx_fd, int16_t qnx_pgrp) {
    int host_fd = i.map_fd(qnx_fd);
    auto pgrp_pid = i.proc().pids().qnx(qnx_pgrp);

    if (!pgrp_pid) {
        return Qnx::QEINVAL;
    } else {
        //printf("setting pgprp to %d from %d\n", pgrp_pid->host_pid(), getpid());
        int r = tcsetpgrp(host_fd, pgrp_pid->host_pid());
        if (r < 0) {
            return Emu::map_errno(errno);
        } else {
            return Qnx::QEOK;
        }
    }
}

uint16_t MainHandler::handle_tcgetpgrp(MsgContext &i, int16_t qnx_fd, int16_t *qnx_pgrp_dst) {
    pid_t host_pgrp = tcgetpgrp(i.map_fd(qnx_fd));
    if (host_pgrp < 0) {
        return Emu::map_errno(errno);
    } else {
        auto pgrp = i.proc().pids().host(host_pgrp);
        if (pgrp == nullptr) {
            *qnx_pgrp_dst = QnxPid::PID_UNKNOWN;
        } else {
            *qnx_pgrp_dst = pgrp->qnx_pid();
        }
        return Qnx::QEOK;
    }
}

void MainHandler::ioctl_terminal_get_size(Ioctl &i) {
    winsize ws;
    int r = ioctl(i.ctx().map_fd(i.fd()), TIOCGWINSZ, &ws);
    Qnx::winsize qws = {
        .ws_row = ws.ws_row,
        .ws_col = ws.ws_col,
        .ws_xpixel = ws.ws_xpixel,
        .ws_ypixel = ws.ws_ypixel,
    };
    i.write(&qws);
    if (r < 0) {
        i.set_retval(Emu::map_errno(errno));
    } else {
        i.set_retval(0);
    }
}

void MainHandler::ioctl_terminal_set_size(Ioctl &i) {
    Qnx::winsize qws;
    i.read(&qws);
    winsize ws = {
        .ws_row = qws.ws_row,
        .ws_col = qws.ws_col,
        .ws_xpixel = qws.ws_xpixel,
        .ws_ypixel = qws.ws_ypixel,
    };
    int r = ioctl(i.ctx().map_fd(i.fd()), TIOCSWINSZ, &ws);
    if (r < 0) {
        i.set_retval(Emu::map_errno(errno));
    } else {
        i.set_retval(0);
    }
}


void ioctl_terminal_set_size(Ioctl &i);

void MainHandler::dev_tcgetattr(MsgContext &i) {
    QnxMsg::dev::tcgetattr_request msg;
    i.msg().read_type(&msg);

    QnxMsg::dev::tcgetattr_reply reply;
    clear(&reply);
    reply.m_status = handle_tcgetattr(i, msg.m_fd, &reply.m_state);
    i.msg().write_type(0, &reply);
}

void MainHandler::dev_tcsetattr(MsgContext &i) {
    QnxMsg::dev::tcsetattr_request msg;
    i.msg().read_type(&msg);

    i.msg().write_status(handle_tcsetattr(i, msg.m_fd, &msg.m_state, msg.m_optional_actions));
}

void MainHandler::dev_term_size(MsgContext &i) {
    QnxMsg::dev::term_size_request msg;
    i.msg().read_type(&msg);
    constexpr uint16_t no_change = std::numeric_limits<uint16_t>::max();
    int host_fd = i.map_fd(msg.m_fd);

    winsize term_winsize;
    int r = ioctl(host_fd, TIOCGWINSZ, &term_winsize);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    QnxMsg::dev::term_size_reply reply;
    clear(&reply);
    reply.m_oldcols = term_winsize.ws_col;
    reply.m_oldrows = term_winsize.ws_row;

    if (msg.m_cols != no_change || msg.m_rows != no_change) {
        if (msg.m_cols != no_change)
            term_winsize.ws_col = msg.m_cols;
        if (msg.m_rows != no_change)
            term_winsize.ws_row = msg.m_rows;

        r = ioctl(host_fd, TIOCSWINSZ, &term_winsize);
        if (r < 0) {
            i.msg().write_status(Emu::map_errno(errno));
            return;
        }
    }

    reply.m_status = Qnx::QEOK;
    i.msg().write_type(0, &reply);
}


void MainHandler::dev_tcgetpgrp(MsgContext &i) {
    QnxMsg::dev::tcgetpgrp_request msg;
    i.msg().read_type(&msg);

    QnxMsg::dev::tcgetpgrp_reply reply;
    clear(&reply);

    reply.m_status = handle_tcgetpgrp(i, msg.m_fd, &reply.m_pgrp);
    i.msg().write_type(0, &reply);
}


void MainHandler::dev_tcsetpgrp(MsgContext &i) {
    QnxMsg::dev::tcsetpgrp_request msg;
    i.msg().read_type(&msg);
    i.msg().write_status(handle_tcsetpgrp(i, msg.m_fd, msg.m_pgrp));
}
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <termios.h>

#include "fd_filter.h"
#include "gen_msg/dev.h"
#include "log.h"
#include "main_handler.h"
#include "msg.h"
#include "process.h"
#include "qnx/ioctl.h"
#include "qnx/term.h"
#include "termios_settings.h"

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
    i.set_status(Qnx::QENOSYS);
}

#define CC_MAP \
    MAP(QVINTR, VINTR) \
    MAP(QVQUIT, VQUIT) \
    MAP(QVERASE, VERASE) \
    MAP(QVKILL, VKILL) \
    MAP(QVEOF, VEOF) \
    MAP(QVEOL, VEOL) \
    MAP(QVSTART, VSTART) \
    MAP(QVSTOP, VSTOP) \
    MAP(QVSUSP, VSUSP) \
    MAP(QVMIN, VMIN) \
    MAP(QVTIME, VTIME) \

#define IFLAG_MAP \
    MAP(QIGNBRK, IGNBRK) \
    MAP(QBRKINT, BRKINT) \
    MAP(QIGNPAR, IGNPAR) \
    MAP(QPARMRK, PARMRK) \
    MAP(QINPCK, INPCK) \
    MAP(QISTRIP, ISTRIP) \
    MAP(QINLCR, INLCR) \
    MAP(QIGNCR, IGNCR) \
    MAP(QICRNL, ICRNL) \
    MAP(QIXOFF, IXOFF) \
    MAP(QIXON, IXON) 

#define OFLAG_MAP \
    MAP(QOPOST, OPOST)

#define CFLAG_MAP \
    MAP(QCSIZE, CSIZE) \
    MAP(QCS5, CS5) \
    MAP(QCS6, CS6) \
    MAP(QCS7, CS7) \
    MAP(QCS8, CS8) \
    MAP(QCSTOPB, CSTOPB) \
    MAP(QCREAD, CREAD) \
    MAP(QPARENB, PARENB) \
    MAP(QPARODD, PARODD) \
    MAP(QHUPCL, HUPCL) \
    MAP(QCLOCAL, CLOCAL)

#define LFLAG_MAP \
    MAP(QISIG, ISIG) \
    MAP(QICANON, ICANON) \
    MAP(QECHO, ECHO) \
    MAP(QECHOE, ECHOE) \
    MAP(QECHOK, ECHOK) \
    MAP(QECHONL, ECHONL) \
    MAP(QNOFLSH, NOFLSH) \
    MAP(QTOSTOP, TOSTOP) \
    MAP(QIEXTEN, IEXTEN)


uint16_t MainHandler::handle_tcgetattr(MsgContext &i, int16_t qnx_fd, Qnx::termios *qnx_attr) {
    struct termios attr;
    int r = tcgetattr(i.map_fd(qnx_fd), &attr);
    if (r < 0) {
        return Emu::map_errno(errno);
    }

    #define MAP(qnx, host) qnx_attr->c_cc[Qnx::qnx] = attr.c_cc[host];
    CC_MAP
    #undef MAP

    #define MAP_FIELD(field, qnx, host) \
        qnx_attr->field &= ~Qnx::qnx; \
        if (attr.field & host) qnx_attr->field |= Qnx::qnx;

    #define MAP(qnx, host) MAP_FIELD(c_iflag, qnx, host)
    IFLAG_MAP
    #undef MAP

    #define MAP(qnx, host) MAP_FIELD(c_oflag, qnx, host)
    OFLAG_MAP
    #undef MAP

    #define MAP(qnx, host) MAP_FIELD(c_cflag, qnx, host)
    CFLAG_MAP
    #undef MAP

    #define MAP(qnx, host) MAP_FIELD(c_lflag, qnx, host)
    LFLAG_MAP
    #undef MAP

    #undef MAP_FIELD

    qnx_attr->c_cflag &= ~(Qnx::QIHFLOW | Qnx::QOHFLOW);
    if (attr.c_cflag & CRTSCTS) {
        qnx_attr->c_cflag |= Qnx::QIHFLOW | Qnx::QOHFLOW;
    }
    
    qnx_attr->c_ispeed = attr.c_ispeed;
    qnx_attr->c_ospeed = attr.c_ospeed;
    return Qnx::QEOK;
}

uint16_t MainHandler::handle_tcsetattr(MsgContext &i, int16_t qnx_fd, const Qnx::termios *qnx_attr, int qnx_action) {
    struct termios attr;
    // get attrs for partial update
    int r = tcgetattr(i.map_fd(qnx_fd), &attr);
    if (r < 0) {
        return Emu::map_errno(errno);
    }

    #define MAP(qnx, host) attr.c_cc[Qnx::qnx]  = qnx_attr->c_cc[host];
    CC_MAP
    #undef MAP

    #define MAP_FIELD(field, qnx, host) \
    attr.field &= ~host; \
    if (qnx_attr->field & Qnx::qnx) attr.field |= host;

    #define MAP(qnx, host) MAP_FIELD(c_iflag, qnx, host)
    IFLAG_MAP
    #undef MAP

    #define MAP(qnx, host) MAP_FIELD(c_oflag, qnx, host)
    OFLAG_MAP
    #undef MAP

    #define MAP(qnx, host) MAP_FIELD(c_cflag, qnx, host)
    CFLAG_MAP
    #undef MAP

    #define MAP(qnx, host) MAP_FIELD(c_lflag, qnx, host)
    LFLAG_MAP
    #undef MAP

    #undef MAP_FIELD

    attr.c_cflag &= ~CRTSCTS;
    if (qnx_attr->c_cflag & (Qnx::QIHFLOW | Qnx::QOHFLOW)) {
        attr.c_cflag |= CRTSCTS;
    }

    attr.c_ispeed = qnx_attr->c_ispeed;
    attr.c_ospeed = qnx_attr->c_ospeed;

    int actions = 0;
    if (qnx_action & Qnx::QTCSANOW)
        actions = TCSANOW;
    if (qnx_action & Qnx::QTCSADRAIN)
        actions = TCSADRAIN;
    if (qnx_action & Qnx::QTCSAFLUSH)
        actions = TCSAFLUSH;

    // not supported on QNX
    attr.c_oflag |= ONLCR;

    r = tcsetattr(i.map_fd(i.m_fd), actions, &attr);
    if (r < 0) {
        return Emu::map_errno(errno);
    } else {
        return Qnx::QEOK;
    }   
}

uint16_t MainHandler::handle_tcsetpgrp(MsgContext &i, int16_t qnx_fd, int16_t qnx_pgrp) {
    int host_fd = i.map_fd(qnx_fd);
    auto pgrp_pid = i.proc().pids().qnx_valid_host(qnx_pgrp);

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

void MainHandler::dev_tcflush(MsgContext &i) {
    QnxMsg::dev::tcflush_request msg;
    i.msg().read_type(&msg);
    int r = tcflush(i.map_fd(msg.m_fd), msg.m_queue);
    i.msg().write_status(Emu::map_errno(r));
}

void MainHandler::dev_tcdrain(MsgContext &i) {
    QnxMsg::dev::tcdrain_request msg;
    i.msg().read_type(&msg);
    int r = tcdrain(i.map_fd(msg.m_fd));
    i.msg().write_status(Emu::map_errno(r));
}

void MainHandler::dev_read(MsgContext &i) {
    int r;
    QnxMsg::dev::read_request msg;
    i.msg().read_type(&msg);

    if (msg.m_proxy != 0) {
        Log::print(Log::UNHANDLED, "dev_read with proxies is not supported");
        i.msg().write_status(Qnx::QENOTSUP);
        return;
    }

    auto fd = i.proc().fds().get_open_fd(msg.m_fd);
    if (fd->m_filter) {
        fd->m_filter->dev_read(i,*fd, msg);
        return;
    }

    TermiosSettings ts(fd->m_host_fd);
    if (!ts.ok()) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    ts.from_dev_read(msg);
    if (!ts.set()) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    QnxMsg::dev::read_reply reply;
    clear(&reply);
    std::vector<struct iovec> iov;
    i.msg().write_iovec(sizeof(reply), msg.m_nbytes, iov);

    r = readv(fd->m_host_fd, iov.data(), iov.size());
    if (r < 0) {
        reply.m_status = Emu::map_errno(errno);
        reply.m_nbytes = 0;
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_nbytes = r;
    }
    i.msg().write_type(0, &reply);
}

void MainHandler::dev_insert_chars(MsgContext &ctx) {
    QnxMsg::dev::insert_chars_request msg;
    int fd = ctx.map_fd(msg.m_fd);
    ctx.msg().read_type(&msg);
    for (int i = 0; i < msg.m_nbytes; i++) {
        char v;
        ctx.msg().read_type(&v, sizeof(msg) + i);
        int r = ioctl(fd, TIOCSTI, v);
        if (r != 0) {
            ctx.msg().write_status(Emu::map_errno(errno));
        }
    }
    ctx.msg().write_status(Qnx::QEOK);
}

void MainHandler::dev_mode(MsgContext &i) {
    QnxMsg::dev::mode_request msg;
    QnxMsg::dev::mode_reply reply;
    i.msg().read_type(&msg);
    int fd = i.map_fd(msg.m_fd);

    struct termios ts;
    int r = tcgetattr(fd, &ts);
    if (r < 0) {
        i.msg().write_status(Emu::map_errno(errno));
        return;
    }

    clear(&reply);

    if (ts.c_lflag & ECHO)
        reply.m_oldmode |= Qnx::DEV_ECHO;
    if (ts.c_lflag & ICANON)
        reply.m_oldmode |= Qnx::DEV_EDIT;
    if (ts.c_lflag & ISIG)
        reply.m_oldmode |= Qnx::DEV_ISIG;
    if (ts.c_oflag & OPOST)
        reply.m_oldmode |= Qnx::DEV_OPOST;

    ts.c_lflag &= ~(ECHO | ICANON | ISIG);
    ts.c_oflag &= ~OPOST;
    // OSFLOW not handled

    if (msg.m_mask & Qnx::DEV_ECHO) {
        ts.c_lflag &= ~(ECHO);
        if (msg.m_mode & Qnx::DEV_ECHO)
            ts.c_lflag |= ECHO;
    }

    if (msg.m_mask & Qnx::DEV_EDIT) {
        ts.c_iflag &= ~(ICRNL | IGNCR | INLCR);
        ts.c_lflag &= ~ICANON;

        if (msg.m_mode & Qnx::DEV_EDIT) {
            ts.c_lflag |= ICANON;
        }
    }

    if (msg.m_mask & Qnx::DEV_ISIG) {
        ts.c_lflag &= ~ISIG;
        if (msg.m_mode & Qnx::DEV_ISIG) {
            ts.c_lflag |= Qnx::DEV_ISIG;
        }
    }

    if (msg.m_mask & Qnx::DEV_OPOST) {
        ts.c_lflag &= ~OPOST;
        if (msg.m_mode & Qnx::DEV_OPOST) {
            ts.c_lflag |= Qnx::DEV_OPOST;
        }
    }
    i.msg().write_status(Emu::map_errno(tcsetattr(fd, TCSANOW, &ts)));
}
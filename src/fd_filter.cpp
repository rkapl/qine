#include <array>
#include <poll.h>
#include "fd_filter.h"
#include "termios_settings.h"
#include "qnx_fd.h"
#include "process.h"

FdFilter::FdFilter() {

}

TerminalFilter::TerminalFilter() {
    m_waiting_for_esc = false;
    m_buttons = 0;
}

TerminalFilter::ReadContext::ReadContext():
    min_bytes(0), max_bytes(0) {
    
}

void TerminalFilter::ReadContext::setup(QnxFd& fd, int read_bytes, const termios& ts) {
    // TODO: check if file is non-blocking
    this->max_bytes = read_bytes;
    if (ts.c_cc[VMIN] == 0) {
        if (ts.c_cc[VTIME] == 0) {
            // poll
            this->min_bytes = 0;
        } else {
            // read at least one byte or timeout
            int r = clock_gettime(CLOCK_MONOTONIC, &deadline);
            assert(r == 0);
            deadline = deadline + Timespec::ms(ts.c_cc[VTIME] * 100);
            this->min_bytes = 1;
        }
    } else {
        this->min_bytes = ts.c_cc[VMIN];
        if (ts.c_cc[VTIME] == 0) {
            // blocking up to VTIME bytes
            this->min_bytes = ts.c_cc[VMIN];
        } else {
            // this is the inter-byte time-out case, just read the first byte without time-out
            // and ignore the rest of the behavior
            this->min_bytes = 1;
        }
    }
}

void TerminalFilter::read(MsgContext& ctx, QnxFd& fd, QnxMsg::io::read_request& msg) {
    termios ts;
    int r = tcgetattr(fd.m_host_fd, &ts);
    assert(r == 0); // it should be a terminal if filter is attached
    TerminalFilter::ReadContext rc;
    rc.setup(fd, msg.m_nbytes, ts);

    QnxMsg::io::read_reply reply;
    MsgHandler::clear(&reply);
    if (!common_read(ctx, fd, rc, &reply.m_status)) {
        ctx.msg().write_type(0, &reply);
    } else {
        reply.m_status = Qnx::QEOK;
        reply.m_nbytes = std::min(m_ready.size(), static_cast<size_t>(msg.m_nbytes));
        ctx.msg().write_type(0, &reply);
        ctx.msg().write(sizeof(reply), m_ready.data(), reply.m_nbytes);
        m_ready.erase(m_ready.begin(), m_ready.begin() + reply.m_nbytes);
    }
}


void TerminalFilter::dev_read(MsgContext& ctx, QnxFd& fd, QnxMsg::dev::read_request& msg) {
    termios ts;
    TermiosSettings::approximate(msg, &ts);
    ReadContext rc;
    rc.setup(fd, msg.m_nbytes, ts);

    QnxMsg::dev::read_reply reply;
    MsgHandler::clear(&reply);
    if (!common_read(ctx, fd, rc, &reply.m_status)) {
        reply.m_status = Emu::map_errno(errno);
        ctx.msg().write_type(0, &reply);
    } else {
        reply.m_nbytes = std::min(m_ready.size(), static_cast<size_t>(msg.m_nbytes));
        ctx.msg().write_type(0, &reply);
        //fprintf(stderr, "reply %d\n", reply.m_nbytes);
        ctx.msg().write(sizeof(reply), m_ready.data(), reply.m_nbytes);
        m_ready.erase(m_ready.begin(), m_ready.begin() + reply.m_nbytes);
    }
}

static const Timespec esc_timeout(Timespec::ms(200));

bool TerminalFilter::common_read(MsgContext& ctx, QnxFd& fd, ReadContext& rc, Qnx::errno_t *errno_out)
{
    // TODO: handle early exit from loop due to pending signals
    // Log::dbg("Reading with min=%d, max=%d, no_timeout=%d\n", (int)rc.min_bytes, (int)rc.max_bytes, (int)rc.no_timeout);
    for (bool first_pass = true;; first_pass = false) {
        bool should_read_data;
        int r;

        Timespec now;
        r = clock_gettime(CLOCK_MONOTONIC, &now);
        assert (r == 0);

        // then check for signals
        if (ctx.proc().emu().should_preempt(errno_out)) {
            return false;
        }

        // first check if we can satisfy request immediately,
        process(now);
        if ((m_ready.size() > 0 || !first_pass) && m_ready.size() >= rc.min_bytes) {
            // Log::dbg("Early exit with buffered data\n");
            return true;
        }

        // then check for timeout
        Deadline deadline = rc.deadline;
        if (m_waiting_for_esc) {
            deadline |= rc.deadline;
        }

        Log::dbg("read min=%d, timeout=%d\n", (int)rc.min_bytes, rc.deadline.is_set());
        pollfd pfd = {
            .fd = fd.m_host_fd,
            .events = POLLIN,
            .revents = 0,
        };
        if (rc.min_bytes == 0) {
            // pure polling case
            r = ppoll(&pfd, 1, &Timespec::ZERO, NULL);
            if (r < 0) {
                *errno_out = Emu::map_errno(errno);
                return false;
            } else {
                should_read_data = r > 0;
            }
        } else if (deadline) {
            // timeout polling case
            //Log::dbg("dd %ld %ld\n", deadline.tv_sec, deadline.tv_nsec);

            if (now >= deadline) {
                return true;
            }
            // and wait (we use poll to provide the timeout functionality) 
            Timespec timeout = deadline - now;
            //Log::dbg("timeout %ld %ld\r\n", timeout.tv_sec, timeout.tv_nsec);
            
            r = ppoll(&pfd, 1, &timeout, NULL);
            if (r < 0) {
                *errno_out = Emu::map_errno(errno);
                return false;
            } else {
                should_read_data = r > 0;
            }
        } else {
            // read without deadline
            should_read_data = true;
        }
        
        if (should_read_data) {
            // read the data
            std::array<char, 128> buf;
            int r = ::read(fd.m_host_fd, buf.data(), buf.size());
            if (r < 0) {
                *errno_out = Emu::map_errno(errno);
                return false;
            }
            m_buffer.insert(m_buffer.end(), buf.begin(), buf.begin() + r);
        }
    }
}

void TerminalFilter::process(const Timespec& now) {
    constexpr char ESC = 27;
    for (;;) {
        size_t i;
        // transfer chunks of not-an-escape sequence
        for (i = 0; i < m_buffer.size(); i++) {
            if (m_buffer[i] == ESC) {
                break;
            }
        }
        transfer(i);
        if (m_buffer.empty()) {
            return;
        }

        // escape sequence now must be at head of the buffer
        //Log::dbg("et %d\r\n", (int)parse_esc());
        switch(parse_esc()) {
            case EscType::RECOGNIZED:
                /* Transfer done be parser */
                m_waiting_for_esc = false;
                break;
            case EscType::UNKNOWN:
                m_waiting_for_esc = false;
                /* Transfer the esc character */
                transfer(1);
                break;
            case EscType::INCOMPLETE: {
                if (m_waiting_for_esc) {
                    if (now >= m_esc_start_time + esc_timeout) {
                        /* Transfer the esc character */
                        transfer(1);
                        m_waiting_for_esc = false;
                        break;
                    } else {
                        /* Continue waiting */
                        return;
                    }
                } else {
                    m_esc_start_time = now;
                    m_waiting_for_esc = true;
                }
            }; break;
        }
    }
}

#define K_MOUSE_BSELECT     0x0400    /* Select button */
#define K_MOUSE_BADJUST     0x0200    /* Adjust button */
#define K_MOUSE_BMENU       0x0100    /* Menu button */
#define K_MOUSE_BUTTONS     (K_MOUSE_BSELECT|K_MOUSE_BADJUST|K_MOUSE_BMENU)

#define MOTION 32
#define DOWN_BTN1 0
#define DOWN_BTN2 1
#define DOWN_BTN3 2
#define BTN_RELEASE 3
#define BTN_MASK 0x3
#define MOD_SHIFT 4
#define MOD_META 8 
#define MOS_CTRL 16
TerminalFilter::EscType TerminalFilter::parse_esc() {
    if (m_buffer.size() < 2) {
        return EscType::INCOMPLETE;
    }

    if (m_buffer[1] == '[') {
        if (m_buffer.size() < 3)
            return EscType::INCOMPLETE;
    } else {
        return EscType::UNKNOWN;
    }

    if (m_buffer[2] == 'M') {
        if (m_buffer.size() < 3 + 3) {
            return EscType::UNKNOWN;
        }        
        int b = m_buffer[3] - 32;
        int column = m_buffer[4] - 33;
        int row = m_buffer[5] - 33;

        char mouse_msg[50];
        char qnx_mods = 0;
        //fprintf(stderr, "b  %d\n", b);
        if (b & MOTION) {
            // motion of held down button
            snprintf(mouse_msg, sizeof(mouse_msg), "\e[%d;%d;%d;%dt", 33, row, column, m_buttons >> 8);
        } else if ((b & BTN_MASK) == BTN_RELEASE) {
            // release of previously held button
            snprintf(mouse_msg, sizeof(mouse_msg), "\e[%d;%d;%d;%dt", 32, row, column, m_buttons >> 8);
            m_buttons = 0;
        } else {
            // button down
            int qnx_btn = 0;
            switch (b & BTN_MASK) {
                case DOWN_BTN1: qnx_btn = K_MOUSE_BSELECT; break;
                case DOWN_BTN2: qnx_btn = K_MOUSE_BADJUST; break;
                case DOWN_BTN3: qnx_btn = K_MOUSE_BMENU; break;
            }
            m_buttons |= qnx_btn;
            int etc = 1; // number of clicks, we are not counting that yet
            snprintf(mouse_msg, sizeof(mouse_msg), "\e[%d;%d;%d;%d;%dt", 31, row, column, qnx_btn >> 8, etc);
        }
        fprintf(stderr, "%s\n", mouse_msg + 1);
        m_ready.insert(m_ready.end(), &mouse_msg[0], &mouse_msg[strlen(mouse_msg)]);
        drop(6);
        return EscType::RECOGNIZED;
    }
    return EscType::UNKNOWN;
}

void TerminalFilter::transfer(size_t size) {
    m_ready.insert(m_ready.end(), m_buffer.begin(), m_buffer.begin() + size);
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + size);
}

void TerminalFilter::drop(size_t size) {
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + size);
}

TerminalFilter::~TerminalFilter() {

}
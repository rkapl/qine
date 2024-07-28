#pragma once

#include "cpp.h"
#include "msg_handler.h"
#include "gen_msg/dev.h"
#include "timespec.h"
#include <termio.h>
#include <vector>

class QnxFd;

/**
 * Attaches to a Qnx FD and hooks the operations
 */
class FdFilter: public NoCopy {
public:
    virtual void read(MsgContext& ctx, QnxFd& fd, QnxMsg::io::read_request& msg) = 0;
    virtual void dev_read(MsgContext& ctx, QnxFd& fd, QnxMsg::dev::read_request& msg) = 0;
    virtual ~FdFilter() {}
protected:
    FdFilter();
};

/**
 * Currently we are emulating a a hybrid that translates only mouse messages.
 *
 * Theoreticacally, libterm supports xterm mouse, but it falls short, vedit is buggy and dragging is not supported.
 */
class TerminalFilter: public FdFilter {
public:
    TerminalFilter();
    void read(MsgContext& ctx, QnxFd& fd, QnxMsg::io::read_request& msg) override;
    void dev_read(MsgContext& ctx, QnxFd& fd, QnxMsg::dev::read_request& msg) override;
    ~TerminalFilter() override;
private:
    /* Pressed down buttons */
    int m_buttons;
    enum class EscType { UNKNOWN, RECOGNIZED, INCOMPLETE };
    struct ReadContext {
        ReadContext();
        /* We do not play the inter-character timeout game */
        Deadline deadline;
        size_t min_bytes;
        size_t max_bytes;

        void setup(QnxFd& fd,   int read_bytes, const termios& settings);
    };

    /**
     * @returns true/false with errno
     */
    bool common_read(MsgContext& ctx, QnxFd& fd, ReadContext& rc, Qnx::errno_t *errno_out);
    /** Move data from buffer to ready, if there is enough data */
    void process(const Timespec& now);
    EscType parse_esc();
    /** Move from buffer to ready */
    void transfer(size_t size);
    /** Remove from buffer head */
    void drop(size_t size);

    /** When did we receive the start of escape sequence, for our own timeout management */
    bool m_waiting_for_esc;
    Timespec m_esc_start_time;
    /** Bufer where we keep the characters needed to interpret the escape sequence, until we can transform them
     * or we can make sure they are not interesting */
    std::vector<uint8_t> m_buffer;
    /** What is ready to be sent to the client  */
    std::vector<uint8_t> m_ready;
};
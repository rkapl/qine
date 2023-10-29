#pragma once

#include "gen_msg/io.h"
#include "msg.h"
#include "qnx/types.h"
#include <cstring>

class GuestContext;


/* All information about the message that was sent by the guest */
struct MsgContext {
    friend class Emu;
public:
    Msg& msg() {return *m_msg;}
    GuestContext& ctx() {return *m_ctx;}
    Process& proc() {return *m_proc;}

    /* Shortcut to map open QNX fd to host FD via fdmap.
     * Throws BadFdException or returns closed linux FD (implementation may change)*/
    int map_fd(Qnx::fd_t qnx_fd);

    bool m_via_fd;
    /* fd or pid*/
    union {
        Qnx::pid_t m_pid;
        int m_fd;
    };
private:
    Process* m_proc;
    GuestContext* m_ctx;
    Msg *m_msg;
};

/* Provides interface to accessing QIOCTL data within a message and returning the result */
class Ioctl {
public:
    Ioctl(MsgContext* m_ctx);
    // Set status and no retval
    inline void set_status(uint16_t v);
    // Set retval and successfull status
    inline void set_retval(uint16_t v);
    uint16_t status() const { return m_status; }
    uint16_t retval() const { return m_retval; }

    size_t request_data_offset() const { return sizeof(QnxMsg::io::qioctl_request); }
    size_t reply_data_offset() const { return sizeof(QnxMsg::io::qioctl_reply); }

    template <class T>
    void write(const T* data) { m_ctx->msg().write_type(reply_data_offset(), data); }

    template <class T>
    void read(T* data) { m_ctx->msg().read_type(data, request_data_offset()); }

    template <class T>
    T read_value();

    void write_response_header();

    uint32_t request_code() const {return m_header.m_request; }
    uint32_t size() const {return m_header.m_nbytes; }
    uint32_t request_group() const {return (m_header.m_request >> 8) & 0xFF; }
    uint32_t request_number() const {return (m_header.m_request) & 0xFF; }
    int16_t fd() const { return m_header.m_fd; }
    MsgContext& ctx() {return *m_ctx;}
private:
    uint16_t m_status;
    uint16_t m_retval;
    MsgContext *m_ctx;
    QnxMsg::io::qioctl_request m_header;
};

template <class T>
T Ioctl::read_value() { 
    T v;
    m_ctx->msg().read_type(&v, request_data_offset()); 
    return v;
}

void Ioctl::set_status(uint16_t v) {
    m_status = v;
    m_retval = 0;
}
    // Set retval and successfull status
void Ioctl::set_retval(uint16_t v) {
    m_status = Qnx::QEOK;
    m_retval = v;
}

/**
 * This corresponds a somewhat to a process in QNX. But we handle the messasages in-proc and provide
 * a lot of meta-info.
 */
class MsgHandler {
public:
    virtual void receive(MsgContext& msg) = 0;

    template<class T>
    static void clear(T *msg) {
        memset(msg, 0, sizeof(*msg));
    }
};

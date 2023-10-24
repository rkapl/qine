#pragma once

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

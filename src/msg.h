#pragma once

#include <cassert>
#include <stddef.h>

#include "emu.h"
#include "types.h"
#include "qnx/types.h"

/* 
 * Encapsulates QNX send/reply message. Tries to hide its segmented nature.
 * 
 * Send refers to the message that "was" sent and rcv refers to the message that is going to be sent by you (the receiver.)
 */
class Msg {
public:
    Msg(Process *proc, size_t m_send_parts, FarPointer m_send, size_t rcv_parts, FarPointer rcv);
    ~Msg();
    void read(void *dst, size_t offset, size_t size);
    template<class T> void read_type(T *dst, size_t offset = 0);
    size_t size() const { return m_size; }

    // Only single-part replies supported for now
    void reply(const void *src, size_t size);
private:
    Process *m_proc;
    size_t m_size;

    size_t m_rcv_parts;
    Qnx::mxfer_entry* m_rcv;

    size_t m_send_parts;
    Qnx::mxfer_entry* m_send;
};

template<class T> void Msg::read_type(T *dst, size_t offset) {
    read(static_cast<void*>(dst), offset, sizeof(T));
}
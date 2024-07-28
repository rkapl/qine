#pragma once

#include <bits/types/FILE.h>
#include <cassert>
#include <cstdint>
#include <stddef.h>
#include <limits>
#include <vector>

#include "guest_context.h"
#include "emu.h"
#include "msg/meta.h"
#include "types.h"
#include "qnx/types.h"

struct iovec;

class MsgStreamReader;

/* 
 * Encapsulates QNX send/reply message. Tries to hide its segmented nature.
 * 
 * Send refers to the message that "was" sent and rcv refers to the message that is going to be sent by you (the receiver.)
 * The names are derived from the sending code's view.
 *
 * You can read and write and parts of the message as you need.
 */
class Msg {
    friend class MsgStreamReader;
public:
    Msg(Process *proc, size_t m_send_parts, FarPointer m_send, size_t rcv_parts, FarPointer rcv, Bitness b);
    ~Msg();

    void read(void *dst, size_t offset, size_t size);
    template<class T> void read_type(T *dst, size_t offset = 0);

    void read_written(void *dst, size_t offset, size_t size);

    size_t write(size_t offset, const void *src, size_t size);
    template<class T> void write_type(size_t offset, const T* src);
    void write_status(uint16_t status);

    /** Get IOVEC for writing into the message */
    void write_iovec(size_t offset, size_t size, std::vector<iovec>& dst);
    /** Get IOVEC for reading from the message */
    void read_iovec(size_t offset, size_t size, std::vector<iovec>& dst);

    void dump_send(FILE *f);
    void dump_structure(FILE *f);
private:
    void translate_mxfer_entries(size_t n, const Qnx::mxfer_entry16 src[], std::vector<Qnx::mxfer_entry>& dst);

    // Iterates through chunks, mainting position within chunk
    class Iterator {
    friend class Msg;
    friend class MsgStreamReader;
    public:
        static constexpr size_t no_limit = std::numeric_limits<size_t>::max();

        /* 
        * Return next part of the message and advance the iterator. The returned part will be up to \size,
        * but may be less depending on chunk. 
        * 
        * Only returns zero slice on EOF.
        */
        FarSlice next(size_t size = no_limit);
        bool eof();
        size_t msg_offset() const;
        /*
         * Skip to a given offset or to end if dst_offset is outside of message.
         */
        void skip_to(size_t dst_offset);
    private:
        Iterator(Qnx::mxfer_entry *chunks, size_t chunk_count);
        // skip empty or iterated-through segments
        void normalize();
        
        Qnx::mxfer_entry *m_chunks;
        size_t m_chunk_count;
        
        // the current chunk
        size_t m_i;
        // and the offset of that chunk within the message
        size_t m_offset_of_chunk;
        // offset within the chunk
        size_t m_offset;
    };

    Iterator iterate_send();
    Iterator iterate_receive();
    void common_iovec(Iterator it, RwOp op, size_t offset, size_t size, std::vector<iovec>& dst);

    Process *m_proc;

    size_t m_rcv_parts;
    Qnx::mxfer_entry* m_rcv;

    std::vector<Qnx::mxfer_entry> m_rcv_translated_entries;

    size_t m_send_parts;
    Qnx::mxfer_entry* m_send;
    std::vector<Qnx::mxfer_entry> m_send_translated_entries;
};

/* Allows reading message byte-by-byte */
class MsgStreamReader {
public:
    inline MsgStreamReader(Msg *msg, size_t skip = 0);
    inline char get(char fallback = 0);
private:
    void get_more();
    Msg *m_msg;
    Msg::Iterator m_it;
    size_t m_ready;
    const uint8_t *m_buf;
};

MsgStreamReader::MsgStreamReader(Msg *msg, size_t skip): 
    m_msg(msg), m_it(msg->m_send, msg->m_send_parts),
    m_ready(0), m_buf(nullptr)
{
    m_it.skip_to(skip);
}

char MsgStreamReader::get(char fallback) {
    if (!m_ready) {
        get_more();
    }

    if (!m_ready) {
        return fallback;
    }

    m_ready--;
    char c = *m_buf;
    m_buf++;
    return c;
}

template<class T> void Msg::read_type(T *dst, size_t offset) {
    read(static_cast<void*>(dst), offset, sizeof(T));
}

template<class T> void Msg::write_type(size_t offset, const T *src) {
    write(offset, static_cast<const void*>(src), sizeof(T));
}
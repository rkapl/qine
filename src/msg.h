#pragma once

#include <bits/types/FILE.h>
#include <cassert>
#include <cstdint>
#include <stddef.h>
#include <limits>
#include <vector>

#include "context.h"
#include "emu.h"
#include "msg/meta.h"
#include "types.h"
#include "qnx/types.h"

struct iovec;

/* 
 * Encapsulates QNX send/reply message. Tries to hide its segmented nature.
 * 
 * Send refers to the message that "was" sent and rcv refers to the message that is going to be sent by you (the receiver.)
 * The names are derived from the sending code's view.
 *
 * You can read and write and parts of the message as you need.
 */
class Msg {
public:
    Msg(Process *proc, size_t m_send_parts, FarPointer m_send, size_t rcv_parts, FarPointer rcv);
    ~Msg();

    void read(void *dst, size_t offset, size_t size);
    template<class T> void read_type(T *dst, size_t offset = 0);

    void read_written(void *dst, size_t offset, size_t size);

    size_t write(size_t offset, const void *src, size_t size);
    template<class T> void write_type(size_t offset, const T* src);
    void write_status(uint16_t status);

    void write_iovec(size_t offset, size_t size, std::vector<iovec>& dst);
    void read_iovec(size_t offset, size_t size, std::vector<iovec>& dst);

    void dump_send(FILE *);
private:
    // Iterates through chunks, mainting position within chunk
    class Iterator {
    friend class Msg;
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

    size_t m_send_parts;
    Qnx::mxfer_entry* m_send;
};

template<class T> void Msg::read_type(T *dst, size_t offset) {
    read(static_cast<void*>(dst), offset, sizeof(T));
}

template<class T> void Msg::write_type(size_t offset, const T *src) {
    write(offset, static_cast<const void*>(src), sizeof(T));
}
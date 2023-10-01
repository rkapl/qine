#include "msg.h"
#include "emu.h"
#include "process.h"
#include "qnx/types.h"
#include "types.h"
#include <algorithm>
#include <cstring>

Msg::Msg(Process* proc, size_t send_parts, FarPointer send, size_t rcv_parts, FarPointer rcv):
    m_proc(proc), 
    m_rcv_parts(rcv_parts),
    m_send_parts(send_parts),
    m_size(0)
{

        m_send = reinterpret_cast<Qnx::mxfer_entry*>(
        proc->translate_segmented(send, sizeof(Qnx::mxfer_entry) * send_parts, RwOp::READ)
    );
    
    m_rcv = reinterpret_cast<Qnx::mxfer_entry*>(
        proc->translate_segmented(rcv, sizeof(Qnx::mxfer_entry) * rcv_parts, RwOp::WRITE)
    );
    
    for (size_t i = 0; i < m_send_parts; i++) {
        m_size += m_send[i].mxfer_len;
    }
}

Msg::~Msg() {

}

void Msg::read(void *dstv, size_t msg_offset, size_t size) {
    // QNX does not actually transfer message lengths, they must be implied or part of the message
    // We copy this in the API and send back garbage for overreads

    Qnx::mxfer_entry *chunk = m_send;
    uint8_t *dst = static_cast<uint8_t*>(dstv);
    size_t chunk_offset = 0;
    size_t chunk_id = 0;

    auto advance_chunk = [&chunk_id, &chunk_offset, &chunk] {
        chunk_offset += chunk->mxfer_len;
        chunk_id++;
        chunk++;
    };

    // skip xfers until we hit intersecting xfer
    while (msg_offset >= chunk_offset + chunk->mxfer_len) {
        advance_chunk();
    }

    while ((size != 0) && (chunk_id < m_send_parts)){
        // where to start reading in this chunk
        size_t chunk_read_offset = 0;
        if (msg_offset > chunk_offset )
            chunk_read_offset = msg_offset - chunk_offset;
        
        // transfer size
        size_t chunk_read_length = std::min(chunk->mxfer_len - chunk_read_offset, size);
        auto chunk_data = static_cast<char*>(m_proc->translate_segmented(FarPointer(chunk->mxfer_seg, chunk->mxfer_off), chunk->mxfer_len));
        memcpy(dst + chunk_read_offset, chunk_data + chunk_read_offset, chunk_read_length);

        advance_chunk();
        dst += chunk_read_length;
        size -= chunk_read_length;
    }

    memset(dst, 0xCC, size);
}
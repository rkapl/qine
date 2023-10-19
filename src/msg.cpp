#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/uio.h>

#include "msg.h"
#include "emu.h"
#include "process.h"
#include "qnx/types.h"
#include "types.h"

Msg::Msg(Process* proc, size_t send_parts, FarPointer send, size_t rcv_parts, FarPointer rcv):
    m_proc(proc), 
    m_rcv_parts(rcv_parts),
    m_send_parts(send_parts)
{
    m_send = reinterpret_cast<Qnx::mxfer_entry*>(
        proc->translate_segmented(send, sizeof(Qnx::mxfer_entry) * send_parts, RwOp::READ)
    );

    m_rcv = reinterpret_cast<Qnx::mxfer_entry*>(
        proc->translate_segmented(rcv, sizeof(Qnx::mxfer_entry) * rcv_parts, RwOp::WRITE)
    );
}

Msg::~Msg() {

}

void Msg::dump_send(FILE *f) {
    auto i = iterate_send();

    for (;;) {
        auto s = i.next(1);
        if (s.is_empty())
            break;
        fprintf(f, "%02X ", *static_cast<uint8_t*>(m_proc->translate_segmented(s.m_ptr)));
    }

    int rs = 0;
    for (size_t i = 0; i < m_rcv_parts; i++)
        rs += m_rcv[i].mxfer_len;
    fprintf(f, ", reply size %d\n", rs);
}

void Msg::dump_structure(FILE *f) {
    size_t start  = 0;
    for (size_t i = 0; i < m_rcv_parts; i++) {
        auto r = &m_rcv[i];
        printf("rcv: %04zx - %04zx -> %04x:%08x\n", start, start + r->mxfer_len, r->mxfer_seg, r->mxfer_off);
        start += r->mxfer_len;
    }

    start  = 0;
    for (size_t i = 0; i < m_send_parts; i++) {
        auto r = &m_send[i];
        printf("snd: %04zx - %04zx -> %04x:%08x\n", start, start + r->mxfer_len, r->mxfer_seg, r->mxfer_off);
        start += r->mxfer_len;
    }
}

auto Msg::iterate_send() -> Iterator{
    return Iterator(m_send, m_send_parts);
}
auto Msg::iterate_receive() -> Iterator {
    return Iterator(m_rcv, m_rcv_parts);
}

Msg::Iterator::Iterator(Qnx::mxfer_entry *chunks, size_t chunk_count):
    m_chunks(chunks), m_chunk_count(chunk_count),
    m_i(0), m_offset_of_chunk(0), m_offset(0)
{   
}

FarSlice Msg::Iterator::next(size_t size)
{
    normalize();
    if (m_i == m_chunk_count)
        return FarSlice(FarPointer::null(), 0);
    
    Qnx::mxfer_entry *c = &m_chunks[m_i];
    size_t to_read = c->mxfer_len - m_offset;
    if (to_read > size) {
        to_read = size;
    }

    FarSlice slice(FarPointer(c->mxfer_seg, c->mxfer_off + m_offset), to_read);
    m_offset += to_read;
    // printf("-- %lx %lx %lx\n", m_i, m_offset_of_chunk, m_offset);
    return slice;
}

void Msg::Iterator::normalize() {
    Qnx::mxfer_entry *c = &m_chunks[m_i];
    while (m_i < m_chunk_count && m_offset >= c->mxfer_len) {
        m_offset_of_chunk += m_offset;
        m_offset = 0;
        m_i++;
        c++;
    }
}

bool Msg::Iterator::eof() {
    normalize();
    return m_i == m_chunk_count;
}

size_t Msg::Iterator::msg_offset() const {
    return m_offset_of_chunk + m_offset;
}

void Msg::Iterator::skip_to(size_t dst_offset) {
    FarSlice s;
    do {
        size_t remaining = dst_offset - msg_offset();
        s = next(remaining);
    } while(!s.is_empty());
}

void Msg::read(void *dstv, size_t msg_offset, size_t size) {
    // QNX does not actually transfer message lengths, they must be implied or part of the message
    // We copy this in the API and send back garbage for overreads
    auto i = iterate_send();
    uint8_t *dst = static_cast<uint8_t*>(dstv);
    i.skip_to(msg_offset);

    while (size) {
        auto slice = i.next(size);
        if (slice.is_empty())
            break;
        auto src = m_proc->translate_segmented(slice.m_ptr, slice.m_size, RwOp::READ);
        memcpy(dst, src, slice.m_size);
        dst += slice.m_size;
        size -= slice.m_size;
    }

    memset(dst, 0xCC, size);
}

void Msg::read_written(void *dstv, size_t msg_offset, size_t size) {
    auto i = iterate_receive();
    uint8_t *dst = static_cast<uint8_t*>(dstv);
    i.skip_to(msg_offset);

    while (size) {
        auto slice = i.next(size);
        if (slice.is_empty())
            break;
        auto src = m_proc->translate_segmented(slice.m_ptr, slice.m_size, RwOp::READ);
        memcpy(dst, src, slice.m_size);
        dst += slice.m_size;
        size -= slice.m_size;
    }

    memset(dst, 0xCC, size);
}

size_t Msg::write(size_t msg_offset, const void *srcv, size_t size)
{
    size_t orig_size = size;
    auto i = iterate_receive();
    const uint8_t *src = static_cast<const uint8_t*>(srcv);
    i.skip_to(msg_offset);

    while (size) {
        auto slice = i.next(size);
        if (slice.is_empty())
            break;
        auto dst = m_proc->translate_segmented(slice.m_ptr, slice.m_size, RwOp::WRITE);
        mempcpy(dst, src, slice.m_size);
        src += slice.m_size;
        size -= slice.m_size;
    }

    return orig_size - size;
}

void Msg::write_status(uint16_t status) {
    write_type(0, &status);
}

static const uint8_t garbage_read[256] = {'X'};
static const uint8_t garbage_write[256] = {0};

void Msg::common_iovec(Iterator i, RwOp op, size_t offset, size_t size, std::vector<iovec>& dst)
{
    i.skip_to(offset);

    while (size != 0) {
        auto slice = i.next(size);
        if (slice.is_empty())
            break;
        auto ptr = m_proc->translate_segmented(slice.m_ptr, slice.m_size, op);
        struct iovec vec;
        vec.iov_base = ptr;
        vec.iov_len = slice.m_size;
        dst.push_back(vec);

        size -= slice.m_size;
    }

    while (size != 0) {
        struct iovec vec;
        vec.iov_base = const_cast<void*>(static_cast<const void*>(garbage_read));
        vec.iov_len = std::min(size, sizeof(garbage_read));
        dst.push_back(vec);
        size -= vec.iov_len;
    }
}

void Msg::read_iovec(size_t offset, size_t size, std::vector<iovec>& dst) {
    common_iovec(iterate_send(), RwOp::READ, offset, size, dst);
}

void Msg::write_iovec(size_t offset, size_t size, std::vector<iovec>& dst) {
    common_iovec(iterate_receive(), RwOp::WRITE, offset, size, dst);
}

void MsgStreamReader::get_more() {
    auto slice = m_it.next();
    if (slice.is_empty()) {
        return;
    }
    m_buf = static_cast<const uint8_t*>(m_msg->m_proc->translate_segmented(slice.m_ptr, slice.m_size, RwOp::READ));
    m_ready = slice.m_size;
}
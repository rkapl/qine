#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "segment.h"
#include "process.h"
#include "mem_ops.h"

#define DEBUG_ALL_WRITEABLE

Segment::Segment(): 
    m_location(nullptr), 
    m_paged_size(0), m_limit_size(0), m_reserved(0), m_shared(false)
{
}

void Segment::reserve(size_t reservation)
{
    assert(m_reserved == 0);
    
    reservation = MemOps::align_page_up(reservation);

    // see https://github.com/wine-mirror/wine/blob/master/loader/preloader.c
    void *l = mmap(NULL, reservation, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE | MAP_32BIT, -1, 0);
    if (m_location == MAP_FAILED) {
        throw std::bad_alloc();
    }
    m_location = l;
    m_paged_size = 0;
    m_reserved = reservation;
}

void Segment::reserve_at(size_t reservation, uintptr_t addr)
{
    assert(m_reserved == 0);
    
    reservation = MemOps::align_page_up(reservation);

    // see https://github.com/wine-mirror/wine/blob/master/loader/preloader.c
    void *l = mmap(reinterpret_cast<void*>(addr), reservation, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE | MAP_FIXED, -1, 0);
    if (m_location == MAP_FAILED) {
        throw std::bad_alloc();
    }
    m_location = l;
    m_paged_size = 0;
    m_reserved = reservation;
}

int Segment::map_prot(Access access)
{
    int prot;
    switch (access) {
        case Access::EXEC_ONLY: 
        case Access::EXEC_READ: 
            prot = PROT_EXEC | PROT_READ;
            break;
        case Access::READ_ONLY:
            prot = PROT_READ;
            break;
        case Access::READ_WRITE:
            prot = PROT_WRITE | PROT_READ;
            break;
        default:
            prot = PROT_NONE;
    }
    #ifdef DEBUG_ALL_WRITEABLE
    if(prot != PROT_NONE)
        prot |= PROT_WRITE;
    #endif
    return prot;
}

void Segment::grow(Access access, size_t new_size)
{
    assert(MemOps::is_page_aligned(new_size));

    if (new_size + m_paged_size > m_reserved) {
        throw std::bad_alloc();
    }

    int prot = map_prot(access);
    void *start = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_location) + m_paged_size);
    void *l = mmap(start, new_size, prot, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
    if (m_location == MAP_FAILED) {
        throw std::bad_alloc();
    }

    for (size_t i = 0; i < new_size / MemOps::PAGE_SIZE; i++) {
        m_bitmap.push_back(true);
    }

    m_paged_size += new_size;
}

void Segment::skip(size_t new_size)
{
    assert(MemOps::is_page_aligned(new_size));

    if (new_size + m_paged_size > m_reserved) {
        throw std::bad_alloc();
    }

    for (size_t i = 0; i < new_size / MemOps::PAGE_SIZE; i++) {
        m_bitmap.push_back(false);
    }

    m_paged_size += new_size;
}

void Segment::change_access(Access access, size_t offset, size_t size)
{
    int r = mprotect(pointer(offset, size), size, map_prot(access));
    assert(r == 0);
}

void Segment::set_limit(size_t limit) {
    assert(limit < m_paged_size);
    m_limit_size = limit;
}

bool Segment::check_bounds(size_t offset, size_t size) const
{
    bool in_bounds = (offset < m_paged_size) && (size <= m_paged_size - offset);
    if (!in_bounds) {
        return false;
    }

    bool ok = true;
    // Probe each page from start and the last page too for good measure
    uint32_t first = MemOps::align_page_down(offset) / MemOps::PAGE_SIZE;
    uint32_t end = MemOps::align_page_up(offset + size) / MemOps::PAGE_SIZE;
    //printf("Pointer request for pages %x : %x\n", first, end);
    for (uint32_t p = first; p < end; p++) {
        ok &= m_bitmap[p];
    }
    return ok;
}

void* Segment::pointer(size_t offset, size_t size)
{
    assert(check_bounds(offset, size));
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_location) + offset);
}

void Segment::make_shared() {
    m_shared = true;
}

bool Segment::is_shared() const {
    return m_shared;
}

Segment::~Segment() {
    if (m_reserved) {
        munmap(m_location, m_reserved);
    }
}

SegmentAllocator::SegmentAllocator(Segment *seg): 
    m_segment(seg), 
    m_offset(seg->paged_size()), 
    m_last_offset(UINT32_MAX)
{
}

SegmentAllocator::~SegmentAllocator() {
}

void SegmentAllocator::alloc(uint32_t size)
{
    m_last_offset = m_offset;
    m_offset += size;
    uint32_t needed = MemOps::align_page_up(m_offset);
    if (needed > m_segment->paged_size()) {
        m_segment->grow(Access::READ_WRITE, needed - m_segment->paged_size());
    }
}

void SegmentAllocator::push_string(const char *str)
{
    auto len = strlen(str) + 1;
    alloc(len);
    memcpy(ptr(), str, len);
}

// Info for manipulating the last allocated chunk
uint32_t SegmentAllocator::offset() const {
    return m_last_offset;
}
void * SegmentAllocator::ptr() const {
    return m_segment->pointer(offset(), 0);
}


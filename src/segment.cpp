#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "segment.h"
#include "process.h"
#include "mem_ops.h"

Segment::Segment(): 
    m_location(nullptr), 
    m_size(0), m_reserved(0)
{
}

void Segment::reserve(size_t reservation)
{
    assert(m_reserved == 0);
    
    reservation = MemOps::align_page_up(reservation);

    // see https://github.com/wine-mirror/wine/blob/master/loader/preloader.c
    void *l = mmap(NULL, reservation, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    if (m_location == MAP_FAILED) {
        throw std::bad_alloc();
    }
    m_location = l;
    m_size = 0;
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
    m_size = 0;
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
    return prot;
}

void Segment::grow(Access access, size_t new_size)
{
    assert(MemOps::is_page_aligned(new_size));

    if (new_size + m_size > m_reserved) {
        throw std::bad_alloc();
    }

    int prot = map_prot(access);
    void *start = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_location) + m_size);
    void *l = mmap(start, new_size, prot, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
    if (m_location == MAP_FAILED) {
        throw std::bad_alloc();
    }

    for (size_t i = 0; i < new_size / MemOps::PAGE_SIZE; i++) {
        m_bitmap.push_back(true);
    }

    m_size += new_size;
}

void Segment::skip(size_t new_size)
{
    assert(MemOps::is_page_aligned(new_size));

    if (new_size + m_size > m_reserved) {
        throw std::bad_alloc();
    }

    for (size_t i = 0; i < new_size / MemOps::PAGE_SIZE; i++) {
        m_bitmap.push_back(false);
    }

    m_size += new_size;
}

void Segment::change_access(Access access, size_t offset, size_t size)
{
    int r = mprotect(pointer(offset, size), size, map_prot(access));
    assert(r == 0);
}

bool Segment::check_bounds(size_t offset, size_t size) const
{
    bool in_bounds = (offset < m_size) && (size <= m_size - offset);
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

Segment::~Segment() {
    if (m_reserved) {
        munmap(m_location, m_reserved);
    }
}
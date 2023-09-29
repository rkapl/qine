#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "segment.h"
#include "process.h"
#include "mem_ops.h"

Segment::Segment(SegmentId id): 
    m_index(id), m_location(nullptr), 
    m_size(0), m_access(Access::INVALID) 
{
}

void Segment::allocate(Access access, size_t size, size_t reservation)
{
    assert(m_reserved == 0);
    
    reservation = MemOps::align_page_up(reservation);

    // see https://github.com/wine-mirror/wine/blob/master/loader/preloader.c
    void *l = mmap(NULL, reservation, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    if (m_location == MAP_FAILED) {
        throw std::bad_alloc();
    }
    m_location = l;
    m_access = access;
    m_size = 0;
    m_allocated_size = 0;
    m_reserved = reservation;

    if (size) {
        grow(size);
    } else {
        update_descriptors();
    }
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

void Segment::grow(size_t new_size)
{
    assert(new_size >= m_size);
    if (new_size == m_size) {
        return;
    }

    auto aligned_new_size = MemOps::align_page_up(new_size);

    if (aligned_new_size == m_allocated_size) {
        update_descriptors();
    }

    int prot = map_prot(m_access);
    void *start = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_location) + m_size);
    size_t alloc = aligned_new_size - m_size;
    void *l = mmap(start, alloc, prot, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
    if (m_location == MAP_FAILED) {
        throw std::bad_alloc();
    }
    m_size = new_size;
    m_allocated_size = aligned_new_size;
    update_descriptors();
}

void Segment::change_access(Access access)
{
    int r = mprotect(m_location, m_allocated_size, map_prot(access));
    assert(r == 0);
    m_access = access;
}

void Segment::update_descriptors() {

}

bool Segment::check_bounds(size_t offset, size_t size) const
{
    return (offset < m_size) && (size <= m_size - offset);
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
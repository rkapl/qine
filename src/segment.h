#pragma once

#include <cstdint>
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include "cpp.h"
#include "types.h"
#include "intrusive_list.h"

/* 
 * A QNX memory seqment. Backed by mmap. Usually reference counted;
 *
 * A QNX memory segment owns a set of pages that back its memory. The API allows the pages to be added 
 * (the grow and skip).
 *
 * For some small segments a precise size reporting via LSL is important.
 */
class Segment: public IntrusiveList::Node<> {
public:
    Segment();
    ~Segment();

    void reserve(size_t reservation);
    void reserve_at(size_t reservation, uintptr_t addr);
    void change_access(Access access, size_t offset, size_t size);
    void grow(Access access, size_t new_size);
    void skip(size_t skip);
    void set_limit(size_t limit);

    bool check_bounds(size_t offset, size_t size) const;
    void* pointer(size_t offset, size_t size);

    GuestPtr location() const  {
        return static_cast<GuestPtr>(reinterpret_cast<uintptr_t>(m_location));
    }
    size_t size() const {
        return m_limit_size ? m_limit_size : m_paged_size;
    }
    size_t paged_size() const {
        return m_paged_size;
    }

    void make_shared();
    bool is_shared() const;
private:
    static int map_prot(Access access);
    void update_descriptors();
    
    void *m_location;
    size_t m_paged_size;
    size_t m_limit_size;
    size_t m_reserved;
    bool m_shared;
    // Store bitmap of valid readable areas
    std::vector<bool> m_bitmap;
};

/* Helper class to allocate small chunks of memory from segment */
class SegmentAllocator {
public:
    SegmentAllocator(Segment *seg);
    ~SegmentAllocator();
    void alloc(uint32_t size);
    void push_string(const char *str);

    // Info for manipulating the last allocated chunk
    uint32_t offset() const;
    void *ptr() const;
private:
    Segment *m_segment;
    uint32_t m_offset;
    uint32_t m_last_offset;
};
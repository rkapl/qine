#pragma once

#include <cstdint>
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <sys/mman.h>
#include "cpp.h"
#include "types.h"
#include "intrusive_list.h"

/* 
 * A QNX memory seqment. Backed by mmap. Usually reference counted;
 *
 * A QNX memory segment owns a set of pages that back its memory. The API allows the pages to be added 
 * (the grow and skip).
 *
 * For some small segments a precise size reporting via LSL is important and supported.
 * Thus segments support paged-based and bytes-based growing.
 */
class Segment: public IntrusiveList::Node<> {
public:
    Segment();
    ~Segment();

    void reserve(size_t reservation);
    void reserve_at(size_t reservation, uintptr_t addr);
    void change_access(int prot, size_t offset, size_t size);
    void grow_paged(int prot, size_t size);
    /**  Grow the limit by X bytes, optionally allocating new pages of READ_WRITE */
    void grow_bytes(size_t size);
    void grow_bytes_64capped(size_t size);
    void skip_paged(size_t skip);

    bool check_bounds(size_t offset, size_t size) const;
    void* pointer(size_t offset, size_t size);

    GuestPtr location() const  {
        return static_cast<GuestPtr>(reinterpret_cast<uintptr_t>(m_location));
    }
    size_t size() const {
        return m_limit_size;
    }
    size_t paged_size() const {
        return m_paged_size;
    }

    void make_shared();
    bool is_shared() const;
    static int map_prot(Access access);
private:
    /* Does not update limit size */
    void grow_paged_internal(int prot, size_t size);
    void update_descriptors();
    
    void *m_location;
    size_t m_paged_size;
    size_t m_limit_size;
    size_t m_reserved;
    bool m_shared;
    // Store bitmap of valid readable areas
    std::vector<bool> m_bitmap;
};

/* Helper class to allocate carve out chunks of memory from data segment, optionally allocatin more heap */
class StartupSbrk {
public:
    StartupSbrk(Segment *seg, uint32_t m_offset);
    ~StartupSbrk();
    void alloc(uint32_t size);
    void align();
    void push_string(const char *str);

    // Info for manipulating the last allocated chunk
    uint32_t offset() const;
    void *ptr() const;

    uint32_t next_offset() const;
private:
    Segment *m_segment;
    uint32_t m_offset;
    uint32_t m_last_offset;
};
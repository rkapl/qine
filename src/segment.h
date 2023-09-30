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
    bool check_bounds(size_t offset, size_t size) const;
    void* pointer(size_t offset, size_t size);

    void* location() const  {
        return m_location;
    }
    size_t size() const {
        return m_size;
    }
private:
    static int map_prot(Access access);
    void update_descriptors();
    
    void *m_location;
    size_t m_size;
    size_t m_reserved;
    // Store bitmap of valid readable areas
    std::vector<bool> m_bitmap;
};

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "cpp.h"
#include "types.h"

class Segment: private NoCopy, NoMove {
public:
    /* must match the QNX exec format */
    enum class Access {
        READ_WRITE = 0,
        READ_ONLY = 1,
        EXEC_READ = 2,
        EXEC_ONLY = 3,

        INVALID = 8,
    };

    Segment(SegmentId id);
    ~Segment();

    void allocate(Access access, size_t size, size_t reservation);
    void change_access(Access access);
    void grow(size_t new_size);
    bool check_bounds(size_t offset, size_t size) const;
    void* pointer(size_t offset, size_t size);
private:
    static int map_prot(Access access);
    void update_descriptors();

    SegmentId m_index;
    Access m_access;
    void *m_location;
    size_t m_size;
    size_t m_allocated_size;
    size_t m_reserved;
};

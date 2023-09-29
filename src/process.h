#pragma once

#include <assert.h>
#include <stddef.h>
#include <vector>
#include <memory>

#include "cpp.h"
#include "types.h"

class Segment;

/* Represents the currently running process, a singleton */
class Process: private NoCopy, NoMove{
public:
    static inline Process* current();
    static void initialize();

    Segment *allocate_segment(SegmentId index);
    Segment *get_segment(SegmentId index);
private:
    Process();
    ~Process();
    static Process* m_current;

    std::vector<std::unique_ptr<Segment>> m_segments;
};

Process* Process::current() {
    assert(m_current);
    return m_current;
}
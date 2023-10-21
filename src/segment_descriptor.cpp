#include <asm/ldt.h>
#include <cerrno>
#include <cstring>
#include <sys/syscall.h>
#include <unistd.h>

#include "log.h"
#include "mem_ops.h"
#include "segment_descriptor.h"
#include "segment.h"

SegmentDescriptor::SegmentDescriptor(SegmentId id, Access access, const std::shared_ptr<Segment>& seg)
    :m_id(id), m_access(access), m_seg(seg) 
{
    update_descriptors();
    Log::print(Log::LOADER, "LDT %d: %x (access %d)\n", id, m_seg->location(), m_access);
}

void SegmentDescriptor::update_descriptors() {
    struct user_desc ud = {0};
    ud.entry_number = m_id;
    ud.base_addr = m_seg->location();
    if (m_seg->size() > 64*1024) {
        ud.limit = m_seg->size() / MemOps::PAGE_SIZE - 1;
        ud.limit_in_pages = 1;
    } else {
        ud.limit = m_seg->size() - 1;
        ud.limit_in_pages = 0;
    }
    ud.seg_32bit = 1;
    ud.useable = 1;
    ud.read_exec_only = 0;
    if (m_access == Access::EXEC_ONLY || m_access == Access::EXEC_READ) {
        // non-conforming code
        ud.contents = 2;
        if (m_access == Access::EXEC_ONLY) {
            ud.read_exec_only = 1;
        }
    } else {
        // data
        ud.contents = 0;
        if (m_access == Access::READ_ONLY) {
            ud.read_exec_only = 1;
        }
    }
    int r = syscall(SYS_modify_ldt, 1, &ud, sizeof(ud));
    if (r != 0) {
        throw std::logic_error(strerror(errno));
    }
}

void SegmentDescriptor::remove_descriptors() {
    struct user_desc ud = {0};
    ud.entry_number = m_id;
    int r = syscall(SYS_modify_ldt, 1, &ud, sizeof(ud));
    if (r != 0) {
        throw std::logic_error(strerror(errno));
    }
}

SegmentDescriptor::~SegmentDescriptor()
{
    if (m_seg) {
        remove_descriptors();
    }
}
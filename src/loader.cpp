#include "loader.h"

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ucontext.h>

#include "process.h"
#include "segment.h"
#include "fd.h"
#include "mem_ops.h"
#include "loader_format.h"
#include "segment_descriptor.h"
#include "types.h"
#include "context.h"

static_assert(sizeof(lmf_header) == 48);
static_assert(sizeof(lmf_record) == 6);

class LoaderFormatException: public std::runtime_error {
public:
    LoaderFormatException(const char *description): std::runtime_error(description) {}
};

static  void panic(const char* error) {
    throw LoaderFormatException(error);
}

struct AbstractLoader {
    AbstractLoader(const lmf_header* hdr, bool slib): m_hdr(hdr), m_slib(slib) {}

    virtual void prepare_segments(uint32_t segment_count) = 0;
    virtual void alloc_segment(uint32_t id, Access access, uint32_t size) = 0;
    virtual void finalize_segments() = 0;

    virtual void* get_load_addr(const lmf_data& ld, size_t data_size) = 0;
    virtual void finalize_loading() = 0;
    virtual std::shared_ptr<Segment> get_segment(uint32_t segment) = 0;

    virtual ~AbstractLoader() {}

    const lmf_header *m_hdr;
    bool m_slib;

    uint32_t m_code_offset = 0;
};

/* Different from the QNX memory segment. In flat model there is only one QNX segment, but usually two loader segments.
 * The segments are placed from image_base linear adress one after another. The load operations refer to these offsets.
 */
struct FlatSegment {
    uintptr_t start;
    uintptr_t size;
    Access type;
};

struct FlatLoader: public AbstractLoader {
    FlatLoader(const lmf_header* hdr, bool slib): AbstractLoader(hdr, slib) {}

    void prepare_segments(uint32_t segment_count) override;
    void alloc_segment(uint32_t id, Access access, uint32_t size) override;
    void finalize_segments() override;
    void *get_load_addr(const lmf_data& ld, size_t data_size) override;
    void finalize_loading() override;
    std::shared_ptr<Segment> get_segment(uint32_t segment) override;

    std::vector<FlatSegment> m_segments;
    uint32_t m_segment_pos;
    uint32_t m_skip;
    uint32_t m_stack;
    uint32_t m_image_base;
    std::shared_ptr<Segment> m_mem;
};

void FlatLoader::prepare_segments(uint32_t segment_count) {
    m_segments.resize(segment_count);
    m_image_base = MemOps::align_page_up(m_hdr->image_base);
    m_segment_pos = m_image_base;
}

void FlatLoader::alloc_segment(uint32_t id, Access access, uint32_t size)
{
    auto& seg = m_segments[id];
    seg.type = access;
    seg.start = m_segment_pos;
    seg.size = MemOps::align_page_up(size);
    m_segment_pos += MemOps::align_page_up(size);

    printf("Segment %d: size=%x, type=%x, linear=%x\n", id, size, access, seg.start);
}

void FlatLoader::finalize_segments() {
    // allocate QNX segment and check layout
    m_stack = MemOps::align_page_up(m_hdr->stack_nbytes);
    m_skip = MemOps::PAGE_SIZE;
    if (m_stack + m_skip > m_image_base) {
        panic("stack does not fit");
    }

    // actual skip can be larger than one page
    m_skip = m_image_base - m_stack;

    m_mem = Process::current()->allocate_segment();
    m_mem->reserve(MemOps::mega(256));

    // Zero page(s) before stack
    m_mem->skip(m_skip);
    // Stack, data and heap (later)
    m_mem->grow(Access::READ_WRITE, m_stack);
    m_mem->grow(Access::READ_WRITE, m_segment_pos - m_image_base);
    m_mem->grow(Access::READ_WRITE, m_hdr->heap_nbytes);
}

void FlatLoader::finalize_loading() {
    auto proc = Process::current();
    auto& mc = proc->startup_context.uc_mcontext;
    // protect the segments & create selectors
    for (uint32_t si = 0;  si < m_segments.size(); ++si) {
        const auto& seg = m_segments[si];
        m_mem->change_access(seg.type, seg.start, seg.size);
    }

    mc.gregs[REG_ESP] = m_skip + m_stack;
    m_code_offset = m_segments[m_hdr->code_index].start + m_hdr->code_offset;
}

std::shared_ptr<Segment> FlatLoader::get_segment(uint32_t segment) {
    return m_mem;
}

void* FlatLoader::get_load_addr(const lmf_data& ld, size_t data_size) {
    if (ld.segment >= m_segments.size())
        panic("data: invalid segment");

    const auto& seg = m_segments[ld.segment];
    return m_mem->pointer(seg.start + ld.offset, data_size);
}

struct LoadedSegment {
    std::shared_ptr<Segment> segment;
    Access final_access;
};

struct SegmentLoader: public AbstractLoader {
    SegmentLoader(const lmf_header* hdr, bool slib): AbstractLoader(hdr, slib) {
        if (!slib) {
            // we are missing e.g. stack allocation
            panic("segmented files are only supported for slib\n");
        }
    }
    void prepare_segments(uint32_t segment_count) override;
    void alloc_segment(uint32_t id, Access access, uint32_t size) override;
    void finalize_segments() override;
    void *get_load_addr(const lmf_data& ld, size_t data_size) override;
    void finalize_loading() override;
    std::shared_ptr<Segment> get_segment(uint32_t segment) override;

    std::vector<LoadedSegment> m_segments;
};

void SegmentLoader::prepare_segments(uint32_t segment_count) {
    m_segments.resize(segment_count);
}

void SegmentLoader::alloc_segment(uint32_t id, Access access, uint32_t size) {
    auto proc = Process::current();
    auto seg = proc->allocate_segment();
    auto aligned_size = MemOps::align_page_up(size);
    seg->reserve(aligned_size);
    seg->grow(Access::READ_WRITE, aligned_size);
    m_segments[id].segment = seg;
    m_segments[id].final_access = access;
    printf("Segment %d: size=%x, type=%x, linear=%p\n", id, size, access, seg->location());
}

void SegmentLoader::finalize_segments() {
    // Nothing to do here
}

void * SegmentLoader::get_load_addr(const lmf_data& ld, size_t data_size) {
    if (ld.segment >= m_segments.size())
        panic("data: invalid segment");

    return m_segments[ld.segment].segment->pointer(ld.offset, data_size);
}

void SegmentLoader::finalize_loading() {
    for(size_t si = 0; si < m_segments.size(); si++) {
        auto &seg = m_segments[si];
        seg.segment->change_access(seg.final_access, 0, seg.segment->size());
    }
    m_code_offset = m_hdr->code_offset;
}

std::shared_ptr<Segment> SegmentLoader::get_segment(uint32_t seg)
{
    return m_segments[seg].segment;
}


#define PTR_AND_VAL(x) reinterpret_cast<char*>(&(x)), (x)
static void check_value(const char* value, lmf_header_with_record *hdr, uint32_t expected, char *got_addr, uint32_t got) {
    /* We get both the value and address to avoid unaligned access shenanigans. Use the macro above. */
    if (expected != got) {
        uint32_t offset = got_addr - reinterpret_cast<char*>(hdr);
        fprintf(stderr, "loader: %s (at 0x%x): expected 0x%x, got 0x%x\n", 
            value, offset, expected, got);
        throw LoaderFormatException("loader value mismatch");
    }
}

static void checked_read(const char* operation, int fd, void *dst, size_t size) {
    int r = read(fd, dst, size);
    if (r < 0) {
        perror(operation);
        exit(1);
    }

    if (r < size) {
        fprintf(stderr, "%s: EOF\n", operation);
        throw LoaderFormatException("EOF");
    }
}

// see watcom, https://github.com/open-watcom/open-watcom-v2/blob/893cbe8abcc479e75fe8e1517dd23817b0317ca4/bld/wl/c/loadqnx.c#L95
// see https://github.com/radareorg/radare2/issues/12664

void load_executable(const char* path, bool slib) {
    printf("Loading %s\n", path);
    auto fd = UniqueFd(open(path, O_CLOEXEC | O_RDONLY));
    auto proc = Process::current();

    if (!fd.valid()) {
        perror("loader: open");
        exit(1);
    }

    // read and check header
    lmf_header_with_record hdr;
    checked_read("loader: read header", fd.get(), reinterpret_cast<void*>(&hdr), sizeof(hdr));

    if (hdr.record.data_nbytes < sizeof(hdr.header)) {
        panic("loader: declared header size too small");
    }
    check_value("rec_type", &hdr, LMF_HEADER_REC, PTR_AND_VAL(hdr.record.rec_type));
    check_value("reserved", &hdr, 0, PTR_AND_VAL(hdr.record.reserved));
    check_value("spare", &hdr, 0, PTR_AND_VAL(hdr.record.spare));
    int expected_flags = _TCF_32BIT;
    check_value("cflags", &hdr, expected_flags, reinterpret_cast<char*>(&hdr.header.cflags), hdr.header.cflags & expected_flags);
    check_value("version", &hdr, QNX_VERSION, PTR_AND_VAL(hdr.header.version));

    if (!(hdr.header.cpu == 386 || hdr.header.cpu == 486)) {
        panic("loader: unsupported cpu");
    }

    uint32_t segment_count = hdr.record.data_nbytes - sizeof(hdr.header);
    if (segment_count % sizeof(uint32_t) != 0) {
        panic("loader: unaligned header size");
    }

    std::unique_ptr<AbstractLoader> loader;
    if (hdr.header.cflags & _TCF_FLAT) {
        loader.reset(new FlatLoader(&hdr.header, slib));
    } else {
        loader.reset(new SegmentLoader(&hdr.header, slib));
    }

    // read segments (tail of the header) and do the layout
    segment_count /= 4;
    loader->prepare_segments(segment_count);
    std::vector<Access> segment_types;
    segment_types.resize(segment_count);
    for (uint32_t si = 0;  si < segment_count; ++si) {
        uint32_t seg_data;
        checked_read("loader: read segments", fd.get(), reinterpret_cast<void*>(&seg_data), sizeof(seg_data));

        uint32_t size = seg_data & 0x0FFFFFFFu;
        uint32_t type = seg_data >> 28;
        loader->alloc_segment(si, Access(type), size);
        segment_types[si] = Access(type);
    }

    loader->finalize_segments();    
    
    // read individual records and parse the interesting ones
    lmf_record rec;
    rec.rec_type = LMF_HEADER_REC;
    bool rw_state = true;
    while (rec.rec_type != LMF_IMAGE_END_REC) {
        checked_read("loader: read record", fd.get(), reinterpret_cast<void*>(&rec), sizeof(rec));
        // printf("record type=%x, len=%x\n", rec.rec_type, rec.data_nbytes);

        bool need_skip = true;
        if (rec.rec_type == LMF_FIXUP_REC) {
            panic("loader: fixup rec unsupported");
        } else if (rec.rec_type == LMF_LINEAR_FIXUP_REC) {
            panic("loader: fixup rec unsupported");
        } else if (rec.rec_type == LMF_RW_END_REC) {
            // We ignore signature for now
            rw_state = false;
        } else if (rec.rec_type == LMF_LOAD_REC) {
            lmf_data ld;
            if (rec.data_nbytes < sizeof(ld)) {
                panic("loader: data: record too short");
            }
            checked_read("loader: read data header", fd.get(), reinterpret_cast<void*>(&ld), sizeof(ld));
            
            auto data_size = rec.data_nbytes - sizeof(ld);
            printf("load data: segment=0x%x, offset=0x%x, size=0x%x\n", ld.segment, ld.offset, data_size);

            void *dst = loader->get_load_addr(ld, data_size);
            checked_read("loader: load data", fd.get(), dst, data_size);
            need_skip = false;
        }

        if (need_skip) {
            if (lseek(fd.get(), rec.data_nbytes, SEEK_CUR) < 0) {
                perror("loader: skip record");
                exit(1);
            }
        }
    }

    auto& mc = proc->startup_context.uc_mcontext;

    uint16_t cs = 0;
    loader->finalize_loading();
    for (uint32_t si = 0; si < segment_count; si++) {
        /* The selector assignment is bit of a guess. Maybe we should also stash them somewhere else? */
        auto seg = loader->get_segment(si);
        auto sd = proc->create_segment_descriptor(segment_types[si], seg);
        if (hdr.header.code_index == si) {
            FarPointer entry(sd->selector(), loader->m_code_offset);
            if (slib) {
                proc->m_load.entry_slib = entry;
            } else {
                proc->m_load.entry_main = entry;
            }
        }
        if (hdr.header.heap_index == si) {
            mc.gregs[REG_DS] = sd->selector();
            proc->m_load.data_segment = sd->id();
        }
        if (hdr.header.stack_index == si) {
            mc.gregs[REG_SS] = sd->selector();
        }
        if (hdr.header.argv_index == si) {
            mc.gregs[REG_ES] = sd->selector();
            mc.gregs[REG_FS] = sd->selector();
            mc.gregs[REG_GS] = sd->selector();
        }
    }
    if (!slib) {
        mc.gregs[REG_EDX] = mc.gregs[REG_ESP] - hdr.header.stack_nbytes;
    }
}

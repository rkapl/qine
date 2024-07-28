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
#include <sys/mman.h>

#include "process.h"
#include "segment.h"
#include "mem_ops.h"
#include "loader_format.h"
#include "segment_descriptor.h"
#include "types.h"
#include "guest_context.h"
#include "log.h"

static_assert(sizeof(lmf_header) == 48, "lmf_header size mimsatch");
static_assert(sizeof(lmf_record) == 6, "lmf_record size mismatch");

static  void panic(const char* error) {
    throw LoaderFormatException(error);
}

struct LoadContext {
    bool m_slib;
    const lmf_header m_hdr;
    LoadInfo *m_info;
};

struct AbstractLoader {
    AbstractLoader(LoadContext *ctx): m_ctx(ctx) {}

    virtual void prepare_segments(uint32_t segment_count) = 0;
    virtual void alloc_segment(uint32_t id, Access access, uint32_t size) = 0;
    virtual void finalize_segments() = 0;

    virtual void* prepare_segment_load(const lmf_data& ld, size_t file_offset, size_t data_size) = 0;
    virtual void finalize_loading() = 0;
    virtual std::shared_ptr<Segment> get_segment(uint32_t segment) = 0;

    virtual ~AbstractLoader() {}

    LoadContext *m_ctx;
};

/* Different from the QNX memory segment. In flat model there is only one QNX segment, but usually two loader segments.
 * The segments are placed from image_base linear adress one after another. The load operations refer to these offsets.
 */
struct FlatSegment {
    uint32_t start;
    uint32_t size;
    Access type;
};

/* Note: the shmem.txt describes the layout of flat and segmented executables */
struct FlatLoader: public AbstractLoader {
    FlatLoader(LoadContext *ctx): AbstractLoader(ctx) {}

    void prepare_segments(uint32_t segment_count) override;
    void alloc_segment(uint32_t id, Access access, uint32_t size) override;
    void finalize_segments() override;
    void *prepare_segment_load(const lmf_data& ld, size_t file_offset, size_t data_size) override;
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
    m_image_base = MemOps::align_page_up(m_ctx->m_hdr.image_base);
    m_segment_pos = m_image_base;
}

void FlatLoader::alloc_segment(uint32_t id, Access access, uint32_t size)
{
    auto& seg = m_segments[id];
    seg.type = access;
    seg.start = m_segment_pos;
    seg.size = MemOps::align_page_up(size);
    m_segment_pos += MemOps::align_page_up(size);

    Log::print(Log::LOADER, "segment %d: size=%x, type=%x, linear=%x\n", id, size, access, seg.start);
}

void FlatLoader::finalize_segments() {
    auto hdr = &m_ctx->m_hdr;
    // allocate QNX segment and check layout
    m_stack = MemOps::align_page_up(hdr->stack_nbytes);
    m_skip = MemOps::PAGE_SIZE;
    if (m_stack + m_skip > m_image_base) {
        panic("stack does not fit");
    }

    // actual skip can be larger than one page
    m_skip = m_image_base - m_stack;

    m_mem = Process::current()->allocate_segment();
    m_mem->reserve(MemOps::mega(512));

    // Guard page(s) before stack
    m_mem->skip_paged(m_skip);
    // Stack, data and heap (later)
    m_mem->grow_paged(PROT_READ | PROT_WRITE, m_stack);
    m_mem->grow_paged(PROT_READ | PROT_WRITE, m_segment_pos - m_image_base);
    auto heap_start = m_mem->size();
    m_mem->grow_paged(PROT_READ | PROT_WRITE, hdr->heap_nbytes);

    auto load = m_ctx->m_info;
    load->stack_low = m_skip;
    load->stack_size = m_stack;
    load->heap_start = heap_start;
    load->entry = m_segments[hdr->code_index].start + hdr->code_offset;
}

void FlatLoader::finalize_loading() {
    // protect the segments & create selectors
    for (uint32_t si = 0;  si < m_segments.size(); ++si) {
        const auto& seg = m_segments[si];
        m_mem->change_access(Segment::map_prot(seg.type), seg.start, MemOps::align_page_up(seg.size));
    }
}

std::shared_ptr<Segment> FlatLoader::get_segment(uint32_t segment) {
    return m_mem;
}

void* FlatLoader::prepare_segment_load(const lmf_data& ld, size_t file_offset, size_t data_size) {
    if (ld.segment >= m_segments.size())
        panic("data: invalid segment");

    const auto& seg = m_segments[ld.segment];
    return m_mem->pointer(seg.start + ld.offset, data_size);
}

struct LoadedSegment {
    std::shared_ptr<Segment> segment;
};

struct SegmentLoader: public AbstractLoader {
    SegmentLoader(LoadContext *ctx): AbstractLoader(ctx) {
    }
    void prepare_segments(uint32_t segment_count) override;
    void alloc_segment(uint32_t id, Access access, uint32_t size) override;
    void finalize_segments() override;
    void *prepare_segment_load(const lmf_data& ld, size_t file_offset, size_t data_size) override;
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
    // QNX segments are more-or-less moveable. But moving is not supported yet
    // So we try to guess and reserve stuff
    if ((m_ctx->m_hdr.cflags & _TCF_32BIT) == 0) {
        seg->reserve(MemOps::kilo(64));
    } else if (m_ctx->m_slib) {
        seg->reserve(MemOps::mega(4));
    } else {
        seg->reserve(MemOps::mega(64));
    }
    seg->grow_bytes(size);
    m_segments[id].segment = seg;
    Log::print(Log::LOADER, "segment %d: size=%x, type=%x, linear=%x\n", id, size, access, seg->location());
}

void SegmentLoader::finalize_segments() {

    auto hdr = &m_ctx->m_hdr;
    if (hdr->stack_index >= m_segments.size())
        throw LoaderFormatException("Stack index specifies invalid index");

    if (hdr->heap_index >= m_segments.size())
        throw LoaderFormatException("Stack index specifies invalid index");

    auto stack_seg = m_segments[hdr->stack_index].segment.get();
    auto stack_start = stack_seg->size();
    stack_seg->grow_bytes_64capped(hdr->stack_nbytes);

    auto heap_seg = m_segments[hdr->heap_index].segment.get();
    auto heap_start = heap_seg->size();
    heap_seg->grow_bytes_64capped(hdr->heap_nbytes);

    auto load = m_ctx->m_info;
    load->stack_low = stack_start;
    load->stack_size = m_ctx->m_hdr.stack_nbytes;
    load->heap_start = heap_start;
    load->entry = m_ctx->m_hdr.code_offset;
}

void * SegmentLoader::prepare_segment_load(const lmf_data& ld, size_t file_offset, size_t data_size) {
    if (ld.segment >= m_segments.size())
        panic("data: invalid segment");

    return m_segments[ld.segment].segment->pointer(ld.offset, data_size);
}

void SegmentLoader::finalize_loading() {
    for(size_t si = 0; si < m_segments.size(); si++) {
        auto &seg = m_segments[si];
        // leave protection RWX for segment loading
    }
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

void loader_load(int fd, LoadInfo *info_out, bool slib) {
    auto proc = Process::current();

    // read and check header
    lmf_header_with_record hdr;
    checked_read("loader: read header", fd, reinterpret_cast<void*>(&hdr), sizeof(hdr));

    if (hdr.record.data_nbytes < sizeof(hdr.header)) {
        panic("loader: declared header size too small");
    }
    check_value("rec_type", &hdr, LMF_HEADER_REC, PTR_AND_VAL(hdr.record.rec_type));
    check_value("reserved", &hdr, 0, PTR_AND_VAL(hdr.record.reserved));
    check_value("spare", &hdr, 0, PTR_AND_VAL(hdr.record.spare));
    check_value("version", &hdr, QNX_VERSION, PTR_AND_VAL(hdr.header.version));

    info_out->bits = ((hdr.header.cflags & _TCF_32BIT) == 0) ? B16 : B32;

    if (!(hdr.header.cpu == 386 || hdr.header.cpu == 486 || hdr.header.cpu == 286)) {
        panic("loader: unsupported cpu");
    }

    uint32_t segment_count = hdr.record.data_nbytes - sizeof(hdr.header);
    if (segment_count % sizeof(uint32_t) != 0) {
        panic("loader: unaligned header size");
    }

    Log::if_enabled(Log::LOADER, [&hdr] (FILE *s) {
        fprintf(s, "flags: %x, cpu: %d, fpu: %d, base: %x\n", 
            hdr.header.cflags, hdr.header.cpu, hdr.header.fpu, hdr.header.image_base);
        fprintf(s, "code %x:%x, stack %x: size %x, heap %x: size %x\n", 
            hdr.header.code_index, hdr.header.code_offset,
            hdr.header.stack_index, hdr.header.stack_nbytes,
            hdr.header.heap_index, hdr.header.heap_nbytes);
    });

    std::unique_ptr<AbstractLoader> loader;
    LoadContext ctx{slib, hdr.header, info_out};
    if (hdr.header.cflags & _TCF_FLAT) {
        loader.reset(new FlatLoader(&ctx));
    } else {
        loader.reset(new SegmentLoader(&ctx));
    }

    // read segments (tail of the header) and do the layout
    segment_count /= 4;
    loader->prepare_segments(segment_count);
    std::vector<Access> segment_types;
    segment_types.resize(segment_count);
    for (uint32_t si = 0;  si < segment_count; ++si) {
        uint32_t seg_data;
        checked_read("loader: read segments", fd, reinterpret_cast<void*>(&seg_data), sizeof(seg_data));

        uint32_t size = seg_data & 0x0FFFFFFFu;
        uint32_t type = seg_data >> 28;
        loader->alloc_segment(si, Access(type), size);
        segment_types[si] = Access(type);
    }

    loader->finalize_segments();    
    
    // read individual records and parse the interesting ones, loading the data if they are data segments
    lmf_record rec;
    rec.rec_type = LMF_HEADER_REC;
    bool rw_state = true;
    std::vector<qnx_reloc_item> reloc_buf;
    while (rec.rec_type != LMF_IMAGE_END_REC) {
        checked_read("loader: read record", fd, reinterpret_cast<void*>(&rec), sizeof(rec));
        // printf("record type=%x, len=%x\n", rec.rec_type, rec.data_nbytes);

        bool need_skip = true;
        if (rec.rec_type == LMF_FIXUP_REC) {
            uint32_t relocs = rec.data_nbytes / sizeof(qnx_reloc_item);
            if (relocs * sizeof(qnx_reloc_item) != rec.data_nbytes)
                throw LoaderFormatException("Unaligned relocation segment size");
            
            size_t reloc_buf_offset = reloc_buf.size();
            reloc_buf.resize(relocs + reloc_buf_offset);
            checked_read("loader: load relocs", fd, &reloc_buf[reloc_buf_offset], relocs * sizeof(qnx_reloc_item));
            need_skip = false;
        } else if (rec.rec_type == LMF_LINEAR_FIXUP_REC) {
            panic("loader: linear fixup rec unsupported");
        } else if (rec.rec_type == LMF_RW_END_REC) {
            // We ignore signature for now
            rw_state = false;
        } else if (rec.rec_type == LMF_LOAD_REC) {
            lmf_data ld;
            if (rec.data_nbytes < sizeof(ld)) {
                panic("loader: data: record too short");
            }
            checked_read("loader: read data header", fd, reinterpret_cast<void*>(&ld), sizeof(ld));
            
            uint32_t data_size = rec.data_nbytes - sizeof(ld);

            off_t offset = lseek(fd, 0, SEEK_CUR);
            if (offset < 0) {
                panic("loader: cannot tell file position\n");
            }
            Log::print(Log::LOADER, "load data: segment=0x%x, offset=0x%x, file=0x%lx, size=0x%x\n", ld.segment, ld.offset, offset, data_size);
            void *dst = loader->prepare_segment_load(ld, offset, data_size);
            checked_read("loader: load data", fd, dst, data_size);
            need_skip = false;
        } else {
            // Log::dbg("Unsupported record: %x\n", rec.rec_type);
        }

        if (need_skip) {
            if (lseek(fd, rec.data_nbytes, SEEK_CUR) < 0) {
                perror("loader: skip record");
                exit(1);
            }
        }
    }


    uint16_t cs = 0;
    loader->finalize_loading();
    std::vector<uint16_t> selectors;
    for (uint32_t si = 0; si < segment_count; si++) {
        /* Now create descriptors for the segments and write the selector information down */
        auto seg = loader->get_segment(si);
        auto sd = proc->create_segment_descriptor(segment_types[si], seg, info_out->bits);
        Log::print(Log::LOADER, "segment %d selector: %x (access %d)\n", si, sd->selector(), static_cast<int>(segment_types[si]));
        selectors.push_back(sd->selector());

        if (hdr.header.code_index == si) 
            info_out->cs = sd->selector();
        
        if (hdr.header.heap_index == si)
            info_out->ds = sd->selector();
            
        if (hdr.header.stack_index == si)
            info_out->ss = sd->selector();

        // Argv index ignore for now. I guess it would be important for some 16-bit executables.
    }

    // perform relocations we gathered
    for (auto r: reloc_buf) {
        auto reloc_what = reinterpret_cast<uint16_t*>(loader->get_segment(r.segment)->pointer(r.reloc_offset, 2));
        uint16_t segment = ((*reloc_what) & ~0x7u) >> 3;
        if (segment >= segment_count) {
            throw LoaderFormatException("relocation out of range");
        }
        uint16_t relocated_value = selectors[segment];
        //Log::dbg("reloc %04x:%08x -- %x => %x\n", r.segment, r.reloc_offset, *reloc_what, relocated_value);
        (*reloc_what) = relocated_value;
    }
}


// if there is shebang, put the interpreter in info_out
// if any error occurs, it is ok, but the info is not filled in
void loader_check_interpreter(int fd, InterpreterInfo *interp_out) {
    // check for shebang
    char shebang[2];
    int r = read(fd, reinterpret_cast<void*>(shebang), sizeof(shebang));
    if (r < 0) {
        return;
    } else if (r < sizeof(shebang)) {
        return;
    }

    if (!(shebang[0] == '#' && shebang[1] == '!')) {
        return;
    }

    constexpr int interp_buf_size = 1024;
    std::unique_ptr<char[]> interp_buf(new char[interp_buf_size]);
    r = read(fd, interp_buf.get(), interp_buf_size - 1);
    if (r < 0) {
        return;
    }
    interp_buf[r] = 0;

    // skip to the interpreter
    char *c = interp_buf.get();
    while (isspace(*c) && *c != 0)
        c++;

    // read the interpreter body
    const char *interp_start = c;
    while(!isspace(*c) && *c != 0)
        c++;
    
    // skip to the args and insert separator
    const char *interp_end = c;
    while (isspace(*c) && *c != 0) {
        *c = 0;
        c++;
    }

    // skip to the end
    const char *args_start = c;
    while (isspace(*c) && *c != 0)
        c++;

    // we inserted the null terminator
    interp_out->interpreter = PathInfo::mk_qnx_path(interp_start);
    interp_out->interpreter_args.clear();
    interp_out->interpreter_args.append(args_start, c - args_start);
    interp_out->has_interpreter = true;
}
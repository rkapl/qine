#include "loader.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ranges>

#include "process.h"
#include "segment.h"
#include "fd.h"
#include "mem_ops.h"
#include "loader_format.h"

static_assert(sizeof(lmf_header) == 48);
static_assert(sizeof(lmf_record) == 6);

static void panic(const char* error) {
    fputs(error, stderr);
    fputc('\n', stderr);
    exit(1);
}


#define PTR_AND_VAL(x) reinterpret_cast<char*>(&(x)), (x)
static void check_value(const char* value, lmf_header_with_record *hdr, uint32_t expected, char *got_addr, uint32_t got) {
    /* We get both the value and address to avoid unaligned access shenanigans. Use the macro above. */
    if (expected != got) {
        uint32_t offset = got_addr - reinterpret_cast<char*>(hdr);
        fprintf(stderr, "loader: %s (at 0x%x): expected 0x%x, got 0x%x\n", 
            value, offset, expected, got);
        exit(1);
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
        exit(1);
    }
}

// see watcom
// see https://github.com/radareorg/radare2/issues/12664

void load_executable(const char* path) {
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

    check_value("version", &hdr, QNX_VERSION, PTR_AND_VAL(hdr.header.version));

    if (!(hdr.header.cpu == 386 || hdr.header.cpu == 486)) {
        panic("loader: unsupported cpu");
    }

    uint32_t segment_count = hdr.record.data_nbytes - sizeof(hdr.header);
    if (segment_count % sizeof(uint32_t) != 0) {
        panic("loader: unaligned header size");
    }

    // read segments (tail of the header)
    segment_count /= 4;
    std::vector<uint8_t> segment_accesses;
    segment_accesses.resize(segment_count);
    for (auto segment_index: std::views::iota(0u, segment_count)) {
        uint32_t seg_data;
        checked_read("loader: read segments", fd.get(), reinterpret_cast<void*>(&seg_data), sizeof(seg_data));

        uint32_t size = seg_data & 0x0FFFFFFFu;
        uint32_t type = seg_data >> 28;
        //printf("Segment %d: size=%x, type=%x\n", segment_index, size, type);

        auto segment = proc->allocate_segment(segment_index);
        uintptr_t reserve = MemOps::mega(16);
        if (type == QNX_READ_WRITE) {
            // probably contains heap
            reserve = MemOps::mega(256);
        }
        segment->allocate(Segment::Access::READ_WRITE, size, std::max(size, reserve));
        segment_accesses[segment_index] = type;
    }
    
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
            //printf("load data: segment=0x%x, offset=0x%x, size=0x%x\n", ld.segment, ld.offset, data_size);

            auto seg = proc->get_segment(ld.segment);
            if (!seg) {
                panic("loader: data: invalid segment");
            }
            if (!seg->check_bounds(ld.offset, data_size)) {
                panic("loader: data: segment overrun");
            }
            void *dst = seg->pointer(ld.offset, data_size);

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

    // protect the segments
    for (auto segment_index: std::views::iota(0u, segment_count)) {
        auto a = Segment::Access(segment_accesses[segment_index]);
        proc->get_segment(segment_index)->change_access(a);
    }
}
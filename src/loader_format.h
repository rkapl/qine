#pragma once

#include <cstdint>
#include "compiler.h"

/* 
* These definitions are taken from https://github.com/open-watcom/open-watcom-v2/blob/master/bld/watcom/h/exeqnx.h
*
* However, I believe that API specifications are not copyrightable and its usage should be fine.
*/

#define QNX_VERSION 400

#define QNX_MAX_REC_SIZE (0x8000 - 512) // was 0xFFFF
#define QNX_MAX_FIXUPS   (0x8000 - 512) // maximum fixup size that can be put
                                        // into one record.
                                        // was 65532

#define QNX_READ_WRITE  0
#define QNX_READ_ONLY   1
#define QNX_EXEC_READ   2
#define QNX_EXEC_ONLY   3

enum {
    LMF_HEADER_REC = 0,
    LMF_COMMENT_REC,
    LMF_LOAD_REC,
    LMF_FIXUP_REC,
    LMF_8087_FIXUP_REC,
    LMF_IMAGE_END_REC,
    LMF_RESOURCE_REC,
    LMF_RW_END_REC,
    LMF_LINEAR_FIXUP_REC
    /* 9  -> 15  reserved for future expansion */
    /* 16 -> 255 for user defined records */
};

struct lmf_record {
    uint8_t      rec_type;
    uint8_t      reserved;       // must be 0
    uint16_t     data_nbytes;    // size of the data record after this.
    uint16_t     spare;          // must be 0
} qine_attribute_packed;

struct lmf_data {
    uint16_t     segment;
    uint32_t     offset;
} qine_attribute_packed;

#define _TCF_LONG_LIVED                 0x0001
#define _TCF_32BIT                      0x0002
#define _TCF_PRIV_MASK                  0x000c
#define _TCF_FLAT                       0x0010

#define SEG16_CODE_FIXUP                0x0004
#define LINEAR32_CODE_FIXUP             0x80000000
#define LINEAR32_SELF_RELATIVE_FIXUP    0x40000000

struct lmf_header {
    uint16_t     version;
    uint16_t     cflags;
    uint16_t     cpu;            // 86,186,286,386,486
    uint16_t     fpu;            // 0, 87,287,387
    uint16_t     code_index;     // segment of code start;
    uint16_t     stack_index;    // segment to put the stack
    uint16_t     heap_index;     // segment to start DS at.
    uint16_t     argv_index;     // segment to put argv & environment.
    uint16_t     spare2[4];      // must be zero;
    uint32_t     code_offset;    // starting offset of code.
    uint32_t     stack_nbytes;   // stack size
    uint32_t     heap_nbytes;    // initial size of heap (optional).
    uint32_t     image_base;     // starting address of image
    uint32_t     spare3[2];
    uint32_t     seg_nbytes[0];  // variable length array of seg. sizes.
} qine_attribute_packed;

struct lmf_header_with_record {
    lmf_record record;
    lmf_header header;
} qine_attribute_packed;

struct lmf_eof {
    uint8_t  spare[6];
} qine_attribute_packed;

/* values for the res_type field in the lmf_resource structure */
enum {
    RES_USAGE = 0
};

struct lmf_resource {
    uint16_t res_type;
    uint16_t spare[3];
} qine_attribute_packed;

struct lmf_rw_end {
    uint16_t     verify;
    uint32_t     signature;
} qine_attribute_packed;

struct qnx_reloc_item {
    uint16_t segment;
    uint32_t reloc_offset;
} qine_attribute_packed;

struct qnx_linear_item {
    uint32_t reloc_offset;
} qine_attribute_packed;

#define VERIFY_OFFSET 36

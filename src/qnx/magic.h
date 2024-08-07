#pragma once

#include "../compiler.h"

#include "types.h"
#include "../types.h"
#include <gen_msg/proc.h>
#include <cstdint>

namespace Qnx {
constexpr uint16_t MAGIC_PTR_SELECTOR = 0x78;
struct MagicPtr {
  uint32_t offset;
  uint32_t segment;
};
struct Magic {
    Qnx::mpid_t            my_pid;
    short int              zero0;
    Qnx::mpid_t            dads_pid;
    short int              zero1;
    Qnx::nid_t             my_nid;
    int32_t                Errno;
    int16_t                my_tid;
    int16_t                zero2[1];
    GuestPtr               zero4[5];          /* offset = 20(0x14) */
 /*-
  * The 16-bit version never declared these members, so programs and
  * lib routines went merily off using strange constants.
  * I changed the declaration to more intuitive names, however some programs
  * (esp sin) were too convoluted to go and ifdef __386__, so sptrs was brought
  * back, however it is only declared as length == 1, so that the other names
  * could remain, with sptrs[n, where n >= 1] would map as:
  *         sptrs[0]     == sptrs[0].
  *         sptrs[1]     == signal table.
  *         sptrs[2]     == cmd
  *         sptrs[3]     == cwd
  *         sptrs[4]     == root_prefix
  *         sptrs[5]     == term state
  *         sptrs[6]     == kernel_ndp_something
  *         sptrs[7]     == windows shared lib global data pointer
  *         sptrs[8]     == codeseg
  *         sptrs[9]     == data/stack seg
  *         sptrs[10]    == &errno.
  *         sptrs[10..20] == unknown_sptrs[0..11]
  */
    GuestPtr                sptrs[1];  /* offset 40 (0x28) */
    GuestPtr                    sigtab;    /* signal table */
    GuestPtr                    cmd;               /* pointer to spawn structure */
    GuestPtr                    cwd;
    GuestPtr                    root_prefix;
    GuestPtr                    termst;
    GuestPtr                    kernel_emt;
    GuestPtr                    qw;
    int                     _cs;
    GuestPtr                    _efgfmt;
    GuestPtrTo<int32_t>           errno_ptr;
    GuestPtr                    unknown_sptrs[9];
    /*
     * this area is used by the termination routines to shut the process
     * down.
     */
    struct Qnx::mxfer_entry      xmsg;
    struct QnxMsg::proc::terminate_request   msg;
    /*
     * this is either the emulator save area, or the 80x87 save area,
     * depending upon which you are running.
     */
    char                     fpsave[1];         /* Variable length */
} ;

struct Magic16 {
    uint16_t                Errno;
    Qnx::mpid_t             my_pid;
    Qnx::mpid_t             dads_pid;
    uint16_t                zero1;
    Qnx::nid_t              my_nid;
    uint16_t                dgroup;
    uint16_t                zero2[2];
    uint16_t                my_tid;
    FarPointer16            malloc,
                             realloc,
                             free,
                             getenv,
                             calloc,
                             sptrs[20];
    struct Qnx::mxfer_entry16      xmsg;
    uint32_t                    xmsg_pad;              /* So same size as _mxfer_entry32 */
    struct QnxMsg::proc::terminate_request   msg;
    char                     fpsave[1];     /* Variable length */
};

static constexpr int SPTRS16_UNKNOWN = 0;
static constexpr int SPTRS16_SLIB16PTR = 0;
static constexpr int SPTRS16_EFGFMT = 1;
static constexpr int SPTRS16_CMD = 2;
static constexpr int SPTRS16_CWD = 3;
static constexpr int SPTRS16_ROOT_PRFX = 4;
static constexpr int SPTRS16_TERM_STATE = 5;
static constexpr int SPTRS16_KER_NDP = 6;
static constexpr int SPTRS16_QWINDOWS = 7;
static constexpr int SPTRS16_32_0 = 8;
static constexpr int SPTRS16_32_1 = 9;
}
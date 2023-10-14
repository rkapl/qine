#include "compiler.h"
#include "types.h"
#include <cstddef>
#include <cstdint>

namespace Qnx {

using time_t = uint32_t;
using clock_t = uint32_t;

struct  tms {
        clock_t tms_utime;
        clock_t tms_stime;
        clock_t tms_cutime;
        clock_t tms_cstime;
} qine_attribute_packed;

struct psinfo {
    int16_t                pid,
                             pid_zero,
                             blocked_on,
                             blocked_on_zero,
                             pid_group,
                             pid_group_zero;
    uint32_t                     flags;
    int16_t                rgid,
                             ruid,
                             egid,
                             euid;
    uint32_t                     sp_reg;
    uint16_t                 ss_reg;
    uint32_t                     magic_off;
    uint16_t                 magic_sel,
                             ldt,
                             umask;
    uint32_t                     signal_ignore,
                             signal_pending,
                             signal_mask,
                             signal_off;
    uint16_t          signal_sel;

    char                     state,
                             zero0,
                             zero0a,
                             priority,
                             max_priority,
                             sched_algorithm;

    uint16_t           sid;
    nid_t                    sid_nid;
    uint16_t           zero1[5];

    union {
        struct {
            uint16_t                 father,
                                     father_zero,
                                     son,
                                     son_zero, 
                                     brother,
                                     brother_zero,
                                     debugger,
                                     debugger_zero,
                                     mpass_pid,
                                     mpass_pid_zero;
            uint16_t                 mpass_sel,
                                     mpass_flags;

            char                     name[100];
            uint16_t                links;
            time_t                   file_time;

            short unsigned           nselectors;
            time_t                   start_time;
            struct tms               times;
            int16_t           mxcount;
            uint32_t           zero2[7];
        } proc qine_attribute_packed;
        struct {
            int16_t               local_pid,
                                     local_pid_zero,
                                     remote_pid,
                                     remote_pid_zero,
                                     remote_vid,
                                     remote_vid_zero;
            nid_t                    remote_nid;
            uint16_t          vidseg,
                                     links;
            char                     substate,
                                     zero_v1;
            uint16_t           zero2[49];
        } vproc qine_attribute_packed;
        struct {
            uint16_t           count,
                                     zero2[50];
        } mproc qine_attribute_packed;
    } qine_attribute_packed;

        uint16_t zero3[12];
} qine_attribute_packed;

// or 104 according to ghidra?
static_assert(offsetof(psinfo, proc.name) == 104 );

}

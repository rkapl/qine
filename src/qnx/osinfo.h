#pragma once

#include "compiler.h"
#include "types.h"
#include <cstdint>

namespace Qnx {
    struct osinfo {
        uint16_t  cpu_speed,          /*  A PC is around 960 */
                        num_procs,
                        tick_size,
                        version,
                        timesel,
                        totmemk,
                        freememk;
        char            primary_monitor,
                        secondary_monitor;
        uint16_t  machinesel;
        uint16_t  disksel;        /* pointer to _diskinfo structure */
        uint32_t   diskoff;
        uint32_t   ssinfo_offset;
        uint16_t  ssinfo_sel,
                        microkernel_size;
        char            release,
                        zero2;
        uint32_t        sflags;
        nid_t           nodename;
        uint32_t        cpu,
                        fpu;
        char            machine[16],
                        bootsrc,
                        zero3[9];
        uint16_t  num_names,
                        num_timers,
                        num_sessions,
                        num_handlers,
                        reserve64k,
                        num_semaphores,
                        prefix_len,
                        zero4[4],
                        max_nodes,
                        proc_freemem,
                        cpu_loadmask,
                        fd_freemem,
                        ldt_freemem,
                        num_fds[3],
                        pidmask,
                        name_freemem;
        uint32_t        top_of_mem;
        uint32_t        freepmem;
        uint32_t        freevmem;
        uint32_t        totpmem;
        uint32_t        totvmem;
        uint32_t        cpu_features;
        uint16_t       zero5[13];
    } qine_attribute_packed;

    struct timesel {
        int32_t nsec;
        int32_t seconds;
        int32_t nsec_inc;
        uint32_t cycles_per_sec;
        uint32_t cycle_lo;
        uint32_t cycle_hi;
        uint32_t cnt8254;    
        uint16_t spare[2];
    };
}

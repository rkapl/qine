#pragma once

#include <stdint.h>
#include "../compiler.h"

/* Taken from Watcom headers. The far pointer to magic is stored in global selector 0x78. */
namespace Qnx {
    static constexpr int QPATH_MAX = 255;
    static constexpr int QPATH_MAX_T = 256;
    static constexpr int QNAME_MAX = 48;
    static constexpr int QNAME_MAX_T = 49;

    typedef int32_t pid_t;            /* Used for process IDs & group IDs */
    typedef int16_t mpid_t;           /* Used for process & group IDs in messages */
    typedef int32_t uid_t;             /* Used for user IDs            */
    typedef int16_t muid_t;          /* used in messages */
    typedef uint16_t msg_t;  /* Used for message passing     */
    typedef int32_t nid_t;      /* Used for network IDs         */

    struct mxfer_entry {
        uint32_t mxfer_off;
        uint16_t mxfer_seg;
        uint16_t mxfer_zero;
        uint32_t mxfer_len;
    } qine_attribute_packed;
};


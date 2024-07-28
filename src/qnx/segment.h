#pragma once
namespace Qnx {
    static constexpr int PMF_DATA_RW = 0x0000;  /*  Data RW                               */
    static constexpr int PMF_DATA_R = 0x0001;  /*  Data R                                */
    static constexpr int PMF_CODE_RX = 0x0002;  /*  Code RX                               */
    static constexpr int PMF_CODE_X = 0x0003;  /*  Code Execute only                     */
    static constexpr int PMF_ACCESS_MASK = 0x3;

    static constexpr int PMF_DMA_SAFE = 0x0004;  /*  Will not cross a 64K segmnt boundry   */
    static constexpr int PMF_VOVERLAY = 0x0004;  /*  Treat addr as virtual not physical    */
    static constexpr int PMF_MODIFY = 0x0008;  /*  Can modify access rights              */
    static constexpr int PMF_HUGE = 0x0010;  /*  Next segment is linked to this one    */
    static constexpr int PMF_GLOBAL_OWN = 0x0020;  /*  If global, make segment owned by me   */
    static constexpr int PMF_ALIGN = 0x0040;  /*  Align segment on a GET/PUT operatn    */
    static constexpr int PMF_GLOBAL = 0x0080;  /*  Make segment global                   */
    static constexpr int PMF_NOCACHE = 0x0100;  /*  Disable caching if possible           */
    static constexpr int PMF_DMA_HIGH = 0x0200;  /*  Allow dma requests above 16 meg       */
    static constexpr int PMF_LINKED = 0x0400;  /*  Segment is a link to an existing seg  */
    static constexpr int PMF_DBBIT = 0x0800;  /*  Set code/stack segment to USE32 deflt */
    static constexpr int PMF_SHARED = 0x1000;  /*  More than one process owns segment    */
    static constexpr int PMF_BORROWED = 0x2000;  /*  Segment which was taken from free lst */
    static constexpr int PMF_LOADING = 0x4000;  /*  Segment which has a cmd loading in it */
    static constexpr int PMF_INUSE = 0x8000;  /*  This segment entry is in use          */

    static constexpr int PMF_ALLOCMASK = 0x03af;  /*  Bits for qnx_segment_alloc_flags      */
    static constexpr int PMF_OVERLAYMASK = 0x03af;  /*  Bits for qnx_segment_overlay_flags    */
    static constexpr int PMF_GETMASK = 0x004b;  /*  Bits for qnx_segment_get              */
    static constexpr int PMF_PUTMASK = 0x00cb;  /*  Bits for qnx_segment_put              */
    static constexpr int PMF_FLAGSMASK = 0x080b;  /*  Bits for qnx_segment_flags            */


}
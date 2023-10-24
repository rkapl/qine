#pragma once

#include <stdint.h>

namespace Qnx {

    static constexpr int IO_HNDL_INFO = 1;        /*  STAT, PATHCONF (not much else)  */
    static constexpr int IO_HNDL_RDDIR = 2;       /*  OPENDIR                         */
    static constexpr int IO_HNDL_CHANGE = 3;      /*  CHMOD/CHOWN (to  owner)         */
    static constexpr int IO_HNDL_UTIME = 4;       /*  UTIME (Any writer)              */
    static constexpr int IO_HNDL_LOAD  = 5;       /*  Loader thread opening executable */
    static constexpr int IO_HNDL_CLOAD = 6;       /*  Loader thread caching executable */

    static constexpr int IO_EFLAG_DIR = 0x01;     /*  Path referenced a directory     */
    static constexpr int IO_EFLAG_DOT = 0x02;     /*  Last component was . or ..      */

    /* Mask for access modes*/
    static constexpr int QO_RDONLY = 000000;  /*  Read-only mode */
    static constexpr int QO_WRONLY = 000001;  /*  Write-only mode */
    static constexpr int QO_RDWR = 000002;  /*  Read-Write mode */
    static constexpr int QO_ACCMODE = 000003;
    static constexpr int QO_NONBLOCK = 000200;  /*  Non-blocking I/O                */
    static constexpr int QO_APPEND = 000010;  /*  Append (writes guaranteed at the end)   */

    static constexpr int QO_DSYNC = 000020;  /*  Data integrity synch    */
    static constexpr int QO_SYNC = 000040;  /*  File integrity synch    */

    static constexpr int QO_PRIV = 010000;

    static constexpr int QO_CREAT = 000400;  /*  Opens with file create      */
    static constexpr int QO_TRUNC = 001000;  /*  Open with truncation        */
    static constexpr int QO_EXCL = 002000;  /*  Exclusive open          */
    static constexpr int QO_NOCTTY = 004000;  /*  Don't assign a controlling terminal */

    /* QNX extensions */
    static constexpr int QO_TEMP = 010000;  /*  Temporary file, don't put to disk   */
    static constexpr int QO_CACHE = 020000;  /*  Cache sequential files too      */

    static constexpr int QO_TEXT = 000000;  /*  Text file   (DOS thing)     */
    static constexpr int QO_BINARY = 000000;  /*  Binary file (DOS thing)     */

}
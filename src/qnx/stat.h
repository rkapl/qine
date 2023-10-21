#pragma once

namespace Qnx {
    /* the basic permissions are the same*/
    static constexpr int QS_IPERMS = 000777;              /*  Permission mask                 */

    static constexpr int QS_IFMT = 0xF000;              /*  Type of file                    */
    static constexpr int QS_IFIFO = 0x1000;              /*  FIFO                            */
    static constexpr int QS_IFCHR = 0x2000;              /*  Character special               */
    static constexpr int QS_IFDIR = 0x4000;              /*  Directory                       */
    static constexpr int QS_IFNAM = 0x5000;              /*  Special named file              */
    static constexpr int QS_IFBLK = 0x6000;              /*  Block special                   */
    static constexpr int QS_IFREG = 0x8000;              /*  Regular                         */
    static constexpr int QS_IFLNK = 0xA000;              /*  Symbolic link                   */
    static constexpr int QS_IFSOCK = 0xC000;              /*  Socket                          */

    static constexpr int QS_ISUID = 004000;              /* set user id on execution         */
    static constexpr int QS_ISGID = 002000;              /* set group id on execution        */
    static constexpr int QS_ISVTX = 001000;              /* sticky bit (does nothing yet)    */
    static constexpr int QS_ENFMT = 002000;              /* enforcement mode locking         */
    static constexpr int QS_QNX_SPECIAL = 040000;          /*  QNX special type                */
    static constexpr int QS_QNX_MQUEUE = 020000;

}
#pragma once

#include <stdint.h>

namespace Qnx {

using errno_t = uint16_t;
/* --- Symbolic names of the error return conditions --- */

constexpr errno_t QEOK = 0;       /* No error                                 */
constexpr errno_t QEPERM = 1;     /* Not owner                                */
constexpr errno_t QENOENT = 2;    /* No such file or directory                */
constexpr errno_t QESRCH = 3;     /* No such process                          */
constexpr errno_t QEINTR = 4;     /* Interrupted system call                  */
constexpr errno_t QEIO = 5;       /* I/O error                                */
constexpr errno_t QENXIO = 6;     /* No such device or address                */
constexpr errno_t QE2BIG = 7;     /* Arg list too big                         */
constexpr errno_t QENOEXEC = 8;   /* Exec format error                        */
constexpr errno_t QEBADF = 9;     /* Bad file number                          */
constexpr errno_t QECHILD = 10;   /* No child processes                       */
constexpr errno_t QEAGAIN = 11;   /* Resource unavailable, try again          */
constexpr errno_t QENOMEM = 12;   /* Not enough space                         */
constexpr errno_t QEACCES = 13;   /* Permission denied                        */
constexpr errno_t QEFAULT = 14;   /* Bad address                              */
constexpr errno_t QENOTBLK = 15;  /* Block device required                    */
constexpr errno_t QEBUSY = 16;    /* Device or resource busy                  */
constexpr errno_t QEEXIST = 17;   /* File exists                              */
constexpr errno_t QEXDEV = 18;    /* Cross-device link                        */
constexpr errno_t QENODEV = 19;   /* No such device                           */
constexpr errno_t QENOTDIR = 20;  /* Not a directory                          */
constexpr errno_t QEISDIR = 21;   /* Is a directory                           */
constexpr errno_t QEINVAL = 22;   /* Invalid argument                         */
constexpr errno_t QENFILE = 23;   /* File table overflow                      */
constexpr errno_t QEMFILE = 24;   /* Too many open files                      */
constexpr errno_t QENOTTY = 25;   /* Not a character device                   */
constexpr errno_t QETXTBSY = 26;  /* Text file busy                           */
constexpr errno_t QEFBIG = 27;    /* File too large                           */
constexpr errno_t QENOSPC = 28;   /* No space left on device                  */
constexpr errno_t QESPIPE = 29;   /* Illegal seek                             */
constexpr errno_t QEROFS = 30;    /* Read-only file system                    */
constexpr errno_t QEMLINK = 31;   /* Too many links                           */
constexpr errno_t QEPIPE = 32;    /* Broken pipe                              */
constexpr errno_t QEDOM = 33;     /* Math argument out of domain of function  */
constexpr errno_t QERANGE = 34;   /* Result too large                         */
constexpr errno_t QENOMSG = 35;   /* No message of desired type               */
constexpr errno_t QEIDRM = 36;    /* Identifier removed                       */
constexpr errno_t QECHRNG = 37;   /* Channel number out of range              */
constexpr errno_t QEL2NSYNC = 38; /* Level 2 not synchronized                 */
constexpr errno_t QEL3HLT = 39;   /* Level 3 halted                           */
constexpr errno_t QEL3RST = 40;   /* Level 3 reset                            */
constexpr errno_t QELNRNG = 41;   /* Link number out of range                 */
constexpr errno_t QEUNATCH = 42;  /* Protocol driver not attached             */
constexpr errno_t QENOCSI = 43;   /* No CSI structure available               */
constexpr errno_t QEL2HLT = 44;   /* Level 2 halted                           */
constexpr errno_t QEDEADLK = 45;  /* Deadlock avoided                         */
constexpr errno_t QENOLCK = 46;   /* No locks available in system             */
constexpr errno_t QENOTSUP = 77;  /* Not supported (1003.4)                   */
constexpr errno_t QENAMETOOLONG = 78; /* Name too long */

/* --- Shared librQary errors --- */

constexpr errno_t QELIBACC = 83;  /* Can't access shared library              */
constexpr errno_t QELIBBAD = 84;  /* Accessing a corrupted shared library     */
constexpr errno_t QELIBSCN = 85;  /* .lib section in a.out corrupted          */
constexpr errno_t QELIBMAX = 86;  /* Attempting to link in too many libraries */
constexpr errno_t QELIBEXEC = 87; /* Attempting to exec a shared library      */

/* ---------------Q-------------- */

constexpr errno_t QENOSYS = 89; /* Unknown system call                      */
constexpr errno_t QELOOP = 90;  /* Too many symbolic link or prefix loops   */
constexpr errno_t QENOTEMPTY = 93;   /* Directory not empty   */
constexpr errno_t QEOPNOTSUPP = 103; /* Operation not supported */
constexpr errno_t QESTALE = 122; /* Potentially recoverable i/o error        */

/* --- Sockets ---Q */

/* non-blocking anQd interrupt i/o */
/*        EAGAIN  Q         35        */  /* Resource temporarily unavailable */
/*        EWOULDBLQOCK      35        */  /* Operation would block */
constexpr errno_t QEWOULDBLOCK = QEAGAIN; /* Operation would block */
constexpr errno_t QEINPROGRESS = 236;     /* Operation now in progress */
/*        EALREADYQ         37        */  /* Operation already in progress */
constexpr errno_t QEALREADY = QEBUSY;     /* Operation already in progress */

/* ipc/network sofQtware -- argument errors */
constexpr errno_t QENOTSOCK = 238;        /* Socket operation on non-socket */
constexpr errno_t QEDESTADDRREQ = 239;    /* Destination address required */
constexpr errno_t QEMSGSIZE = 240;        /* Message too long */
constexpr errno_t QEPROTOTYPE = 241;      /* Protocol wrong type for socket */
constexpr errno_t QENOPROTOOPT = 242;     /* Protocol not available */
constexpr errno_t QEPROTONOSUPPORT = 243; /* Protocol not supported */
constexpr errno_t QESOCKTNOSUPPORT = 244; /* Socket type not supported */
constexpr errno_t QEPFNOSUPPORT = 246;    /* Protocol family not supported */
constexpr errno_t QEAFNOSUPPORT =
    247; /* Address family not supported by protocol family */
constexpr errno_t QEADDRINUSE = 248;    /* Address already in use */
constexpr errno_t QEADDRNOTAVAIL = 249; /* Can't assign requested address */

/* ipc/network sofQtware -- operational errors */
constexpr errno_t QENETDOWN = 250;     /* Network is down */
constexpr errno_t QENETUNREACH = 251;  /* Network is unreachable */
constexpr errno_t QENETRESET = 252;    /* Network dropped connection on reset */
constexpr errno_t QECONNABORTED = 253; /* Software caused connection abort */
constexpr errno_t QECONNRESET = 254;   /* Connection reset by peer */
constexpr errno_t QENOBUFS = 255;      /* No buffer space available */
constexpr errno_t QEISCONN = 256;      /* Socket is already connected */
constexpr errno_t QENOTCONN = 257;     /* Socket is not connected */
constexpr errno_t QESHUTDOWN = 258;    /* Can't send after socket shutdown */
constexpr errno_t QETOOMANYREFS = 259; /* Too many references: can't splice */
constexpr errno_t QETIMEDOUT = 260;    /* Connection timed out */
constexpr errno_t QECONNREFUSED = 261; /* Connection refused */

/*      ELOOP     Q     62        */ /* Too many levels of symbolic links */
/*      ENAMETOOLOQNG   63        */ /* File name too long */

constexpr errno_t QEHOSTDOWN = 264;    /* Host is down */
constexpr errno_t QEHOSTUNREACH = 265; /* No route to host */

/* Network File SyQstem */
/*      ESTALE    Q       70     */     /* Stale NFS file handle */
constexpr errno_t QEREMOTE = 271;       /* Too many levels of remote in path */
constexpr errno_t QEBADRPC = 272;       /* RPC struct is bad */
constexpr errno_t QERPCMISMATCH = 273;  /* RPC version wrong */
constexpr errno_t QEPROGUNAVAIL = 274;  /* RPC prog. not avail */
constexpr errno_t QEPROGMISMATCH = 275; /* Program version wrong */
constexpr errno_t QEPROCUNAVAIL = 276;  /* Bad procedure for program */

/* --- QNX 4.0 specific --- */

constexpr errno_t QENOREMOTE = 1000; /* Must be done on local machine */
constexpr errno_t QENONDP = 1001; /* Need an NDP (8087...) to run            */
constexpr errno_t QEBADFSYS = 1002; /* Corrupted file system detected */
constexpr errno_t QENO32BIT = 1003; /* 32-bit integer fields were used */
constexpr errno_t QENOVPE = 1004;  /* No proc entry avail for virtual process */
constexpr errno_t QENONETQ = 1005; /* Process manager-to-net enqueuing failed */
constexpr errno_t QENONETMAN = 1006; /* Could not find net manager for node no. */
constexpr errno_t QEVIDBUF2SML = 1007; /* Told to allocate a vid buf too small    */
constexpr errno_t QEVIDBUF2BIG = 1008; /* Told to allocate a vid buf too big */
constexpr errno_t QEMORE = 1009; /* More to do, send message again          */
constexpr errno_t QECTRLTERM = 1010; /* Remap to the controlling terminal */
constexpr errno_t QENOLIC = 1011; /* No license                              */
constexpr errno_t QEDSTFAULT = 1012; /* Destination fault on msg pass */

}; // namespace Qnx
#pragma once

#include <stdint.h>

#include "types.h"
#include "term.h"

namespace Qnx {

struct winsize {
    uint16_t      ws_row;         /* rows, in characters */
    uint16_t      ws_col;         /* columns, in characters */
    uint16_t      ws_xpixel;      /* horizontal size, pixels */
    uint16_t      ws_ypixel;      /* vertical size, pixels */
} qine_attribute_packed;

struct ttysize {
    uint16_t      ts_lines;
    uint16_t      ts_cols;
    uint16_t      ts_xxx;
    uint16_t      ts_yyy;
} qine_attribute_packed;

struct sgttyb {
    char sg_ispeed; /* input speed */
    char sg_ospeed; /* output speed */
    char sg_erase;  /* erase character */
    char sg_kill;   /* kill character */
    int32_t  sg_flags;  /* mode flags */
} qine_attribute_packed;

/*
 * TIOCGETC/TIOCSETC structure
 */
struct tchars {
    char t_intrc;   /* interrupt */
    char t_quitc;   /* quit */
    char t_startc;  /* start output */
    char t_stopc;   /* stop output */
    char t_eofc;    /* end-of-file */
    char t_brkc;    /* input delimiter (like nl) */
} qine_attribute_packed;

/*
 * TIOCGLTC/TIOCSLTC structure
 */
struct ltchars {
    char    t_suspc;    /* stop process signal */
    char    t_dsuspc;   /* delayed stop process signal */
    char    t_rprntc;   /* reprint line */
    char    t_flushc;   /* flush output (toggles) */
    char    t_werasc;   /* word erase */
    char    t_lnextc;   /* literal next character */
} qine_attribute_packed;


static constexpr uint32_t QIOCPARM_MASK = 0x1fff;          /* parameter length, at most 13 bits */

static constexpr uint32_t QIOC_VOID = 0x20000000uL;     /* no parameters */
static constexpr uint32_t QIOC_OUT = 0x40000000uL;     /* copy out parameters */
static constexpr uint32_t QIOC_IN = 0x80000000uL;     /* copy in parameters */
static constexpr uint32_t QIOC_INOUT = (QIOC_IN|QIOC_OUT);
static constexpr uint32_t QIOC_DIRMASK = 0xe0000000L;     /* mask for IN/OUT/VOID */

static constexpr uint32_t ioctl_param(uint32_t full) {return full & 0xFFFFu;}

constexpr uint32_t Q_IOC(uint32_t inout, uint32_t group, uint32_t num, uint32_t len) {
    return (inout | (((long)len & QIOCPARM_MASK) << 16) | ((group) << 8) | (num));
}

constexpr uint32_t Q_IO(uint32_t g, uint32_t n) {
    return Q_IOC(QIOC_VOID, g, n, 0);
}

template <class T>
constexpr uint32_t Q_IOR(uint32_t g, uint32_t n) {
    return Q_IOC(QIOC_OUT, g, n, sizeof(T));
}

template <class T>
constexpr uint32_t Q_IOW(uint32_t g, uint32_t n) {
    return Q_IOC(QIOC_IN, g, n, sizeof(T));
}

template <class T>
constexpr uint32_t _IORW(uint32_t g, uint32_t n) {
    return Q_IOC(QIOC_INOUT, g, n, sizeof(T));
}

static constexpr uint32_t QTCGETA = Q_IOR<termio>('T', 1);
static constexpr uint32_t QTCSETA = Q_IOW<termio>('T', 2);
static constexpr uint32_t QTCSETAW = Q_IOW<termio>('T', 3);
static constexpr uint32_t QTCSETAF = Q_IOW<termio>('T', 4);
static constexpr uint32_t QTCSBRK = Q_IO('T', 5);
static constexpr uint32_t QTCXONC = Q_IO('T', 6);
static constexpr uint32_t QTCFLSH = Q_IO('T', 7);
static constexpr uint32_t QTCGETS = Q_IOR<termios>('T', 8);
static constexpr uint32_t QTCSETS = Q_IOW<termios>('T', 9);
static constexpr uint32_t QTCSETSW = Q_IOW<termios>('T', 10);
static constexpr uint32_t QTCSETSF = Q_IOW<termios>('T', 11);

static constexpr uint32_t QTIOCHPCL = Q_IO('t', 2);                    /* hang up on last close */
static constexpr uint32_t QTIOCGETP = Q_IOR<sgttyb>('t', 8);     /* get parameters -- gtty */
static constexpr uint32_t QTIOCSETP = Q_IOW<sgttyb>('t', 9);     /* set parameters -- stty */
static constexpr uint32_t QTIOCSETN = Q_IOW<sgttyb>('t',10);     /* as above, but no flushtty*/

static constexpr uint32_t QTIOCEXCL = Q_IO('t', 13);                    /* set exclusive use of tty */
static constexpr uint32_t QTIOCNXCL = Q_IO('t', 14);                    /* reset exclusive use of tty */
/* 15 unused */
static constexpr uint32_t QTIOCFLUSH = Q_IOW<uint16_t>('t', 16);            /* flush buffers */
static constexpr uint32_t QTIOCSETC = Q_IOW<tchars>('t', 17);    /* set special characters */
static constexpr uint32_t QTIOCGETC = Q_IOR<tchars>('t', 18);    /* get special characters */
static constexpr uint32_t QTIOCGETA = Q_IOR<termios>('t', 19);   /* get termios struct */
static constexpr uint32_t QTIOCSETA = Q_IOW<termios>('t', 20);   /* set termios struct */
static constexpr uint32_t QTIOCSETAW = Q_IOW<termios>('t', 21);   /* drain output, set */
static constexpr uint32_t QTIOCSETAF = Q_IOW<termios>('t', 22);   /* drn out, fls in, set */
//      TIOCGETD        Q_IOR('t', 26, short)            /* get line discipline */
//      TIOCSETD        Q_IOW('t', 27, short)            /* set line discipline */
static constexpr uint32_t QTIOCDRAIN = Q_IO('t', 94);                   /* wait till output drained */
//      TIOCSIG         Q_IO('t',  95)                   /* pty: generate signal */
//      TIOCEXT         Q_IOW('t', 96, short)            /* pty: external processing */
static constexpr uint32_t QTIOCSCTTY = Q_IO('t', 97);                   /* become controlling tty */
//      TIOCCONS        Q_IOW('t', 98, short)            /* become virtual console */
//      TIOCUCNTL       Q_IOW('t', 102, short)           /* pty: set/clr usr cntl mode */
static constexpr uint32_t QTIOCSWINSZ = Q_IOW<winsize>('t', 103);  /* set window size */
static constexpr uint32_t QTIOCGWINSZ = Q_IOR<winsize>('t', 104);  /* get window size */
//      TIOCREMOTE      Q_IOW('t', 105, short)           /* remote input editing */
static constexpr uint32_t QTIOCMGET = Q_IOR<uint16_t>('t', 106);           /* get all modem bits */
static constexpr uint32_t QTIOCMBIC = Q_IOW<uint16_t>('t', 107);           /* bic modem bits */
static constexpr uint32_t QTIOCMBIS = Q_IOW<uint16_t>('t', 108);           /* bis modem bits */
static constexpr uint32_t QTIOCMSET = Q_IOW<uint16_t>('t', 109);           /* set all modem bits */
static constexpr uint32_t QTIOCSTART = Q_IO('t', 110);                  /* start output, like ^Q */
static constexpr uint32_t QTIOCSTOP = Q_IO('t', 111);                  /* stop output, like ^S */
static constexpr uint32_t QTIOCPKT = Q_IOW<uint16_t>('t', 112);           /* pty: set/clear packet mode */

static constexpr uint32_t QTIOCPKT_DATA = 0x00;            /* data packet */
static constexpr uint32_t QTIOCPKT_FLUSHREAD = 0x01;            /* flush packet */
static constexpr uint32_t QTIOCPKT_FLUSHWRITE = 0x02;            /* flush packet */
static constexpr uint32_t QTIOCPKT_STOP = 0x04;            /* stop output */
static constexpr uint32_t QTIOCPKT_START = 0x08;            /* start output */
static constexpr uint32_t QTIOCPKT_NOSTOP = 0x10;            /* no more ^S, ^Q */
static constexpr uint32_t QTIOCPKT_DOSTOP = 0x20;            /* now do ^S ^Q */
static constexpr uint32_t QTIOCPKT_IOCTL = 0x40;            /* state change of pty driver */

static constexpr uint32_t QTIOCNOTTY = Q_IO('t', 113);                   /* void tty association */
static constexpr uint32_t QTIOCSTI = Q_IOW<char>('t', 114);            /* simulate terminal input */
static constexpr uint32_t QTIOCOUTQ = Q_IOR<uint16_t>('t', 115);           /* output queue size */
static constexpr uint32_t QTIOCGLTC = Q_IOR<ltchars>('t', 116);  /* get local special chars*/
static constexpr uint32_t QTIOCSLTC = Q_IOW<ltchars>('t', 117);  /* set local special chars*/
static constexpr uint32_t QTIOCSPGRP = Q_IOW<uint16_t>('t', 118);           /* set pgrp of tty */
static constexpr uint32_t QTIOCGPGRP = Q_IOR<uint16_t>('t', 119);           /* get pgrp of tty */
static constexpr uint32_t QTIOCCDTR = Q_IO('t', 120);                   /* clear data terminal ready */
static constexpr uint32_t QTIOCSDTR = Q_IO('t', 121);                   /* set data terminal ready */
static constexpr uint32_t QTIOCCBRK = Q_IO('t', 122);                   /* clear break bit */
static constexpr uint32_t QTIOCSBRK = Q_IO('t', 123);                   /* set break bit */
static constexpr uint32_t QTIOCLGET = Q_IOR<uint16_t>('t', 124);           /* get local modes */
static constexpr uint32_t QTIOCLSET = Q_IOW<uint16_t>('t', 125);           /* set entire local mode word */

static constexpr uint32_t QTIOCSETPGRP = Q_IOW<uint16_t>('t', 130);           /* set pgrp of tty (posix) */
static constexpr uint32_t QTIOCGETPGRP = Q_IOR<uint16_t>('t', 131);           /* get pgrp of tty (posix) */

static const uint32_t QUIOCCMD(int n) { return Q_IO('u', n); }             /* usr cntl op "n" */

static constexpr uint32_t QFIOCLEX = Q_IO('f', 1);                     /* set close on exec on fd */
static constexpr uint32_t QFIONCLEX = Q_IO('f', 2);                     /* remove close on exec */
static constexpr uint32_t QFIOGETOWN = Q_IOR<uint16_t>('f', 123);           /* get owner */
static constexpr uint32_t QFIOSETOWN = Q_IOW<uint16_t>('f', 124);           /* set owner */
static constexpr uint32_t QFIOASYNC = Q_IOW<uint16_t>('f', 125);           /* set/clear async i/o */
static constexpr uint32_t QFIONBIO = Q_IOW<uint16_t>('f', 126);           /* set/clear non-blocking i/o */
static constexpr uint32_t QFIONREAD = Q_IOR<uint16_t>('f', 127);           /* get # bytes to read */

}
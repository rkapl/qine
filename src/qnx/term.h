#pragma once

namespace Qnx {

static constexpr int Q_NCC = 18;
static constexpr int Q_NCCS = 40;

typedef unsigned short  cc_t;
typedef long            speed_t;
typedef unsigned long   tcflag_t;

struct termio {
    unsigned short     c_iflag;        /* input modes */
    unsigned short     c_oflag;        /* output modes */
    unsigned short     c_cflag;        /* control modes */
    unsigned short     c_lflag;        /* line discipline modes */
    char               c_line;         /* line discipline */
    unsigned short     c_cc[Q_NCC];      /* control chars */
};

struct termios {
    tcflag_t        c_iflag;    /* Input Modes */
    tcflag_t        c_oflag;    /* Ouput modes */
    tcflag_t        c_cflag;    /* Control Modes */
    tcflag_t        c_lflag;    /* Local Modes */
    cc_t            c_cc[Q_NCCS]; /* Control Characters */
    unsigned short  zero[3];
    unsigned short  c_status;   /* device status */
    unsigned short  c_qflag;    /* QNX Specific flags */
    unsigned short  handle;
    speed_t         c_ispeed;   /* Input Baud rate */
    speed_t         c_ospeed;   /* Output baud rate */
};

}
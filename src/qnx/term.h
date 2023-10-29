#pragma once

#include "types.h"

namespace Qnx {

static constexpr int Q_NCC = 18;
static constexpr int Q_NCCS = 40;

typedef uint16_t  cc_t;
typedef int32_t            speed_t;
typedef uint32_t   tcflag_t;

struct termio {
    unsigned short     c_iflag;        /* input modes */
    unsigned short     c_oflag;        /* output modes */
    unsigned short     c_cflag;        /* control modes */
    unsigned short     c_lflag;        /* line discipline modes */
    char               c_line;         /* line discipline */
    unsigned short     c_cc[Q_NCC];      /* control chars */
} qine_attribute_packed;

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
} qine_attribute_packed;

static constexpr int QIGNBRK = 0x0001;
static constexpr int QBRKINT = 0x0002;
static constexpr int QIGNPAR = 0x0004;
static constexpr int QPARMRK = 0x0008;
static constexpr int QINPCK = 0x0010;
static constexpr int QISTRIP = 0x0020;
static constexpr int QINLCR = 0x0040;
static constexpr int QIGNCR = 0x0080;
static constexpr int QICRNL = 0x0100;
static constexpr int QIXON = 0x0400;
static constexpr int QIXOFF = 0x1000;

/*
 * Ouput Modes
 */

static constexpr int QOPOST = 0x0001;

/*
 * Control Modes
 */

static constexpr int QIHFLOW = 0x0001;
static constexpr int QOHFLOW = 0x0002;
static constexpr int QPARSTK = 0x0004;
static constexpr int QIIDLE = 0x0008;

static constexpr int QCSIZE = 0x0030;
static constexpr int QCS5 = 0x0000;
static constexpr int QCS6 = 0x0010;
static constexpr int QCS7 = 0x0020;
static constexpr int QCS8 = 0x0030;
static constexpr int QCSTOPB = 0x0040;
static constexpr int QCREAD = 0x0080;
static constexpr int QPARENB = 0x0100;
static constexpr int QPARODD = 0x0200;
static constexpr int QHUPCL = 0x0400;
static constexpr int QCLOCAL = 0x0800;

/*
 * Local Modes
 */

static constexpr int QISIG = 0x0001;
static constexpr int QICANON = 0x0002;
static constexpr int QECHO = 0x0008;
static constexpr int QECHOE = 0x0010;
static constexpr int QECHOK = 0x0020;
static constexpr int QECHONL = 0x0040;
static constexpr int QNOFLSH = 0x0080;
static constexpr int QTOSTOP = 0x0100;
static constexpr int QIEXTEN = 0x8000;


/*
 * Special Control Character indices into c_cc[]
 */

static constexpr int QVINTR = 0;
static constexpr int QVQUIT = 1;
static constexpr int QVERASE = 2;
static constexpr int QVKILL = 3;
static constexpr int QVEOF = 4;
static constexpr int QVEOL = 5;
static constexpr int QVSTART = 8;
static constexpr int QVSTOP = 9;
static constexpr int QVSUSP = 10;
static constexpr int QVDSUSP = 11;
static constexpr int QVMIN = 16;
static constexpr int QVTIME = 17;

static constexpr int QVFWD = 18;
static constexpr int QVLOGIN = 19;
static constexpr int QVPREFIX = 20;
static constexpr int QVSUFFIX = 24;
static constexpr int QVLEFT = 28;
static constexpr int QVRIGHT = 29;
static constexpr int QVUP = 30;
static constexpr int QVDOWN = 31;
static constexpr int QVINS = 32;
static constexpr int QVDEL = 33;
static constexpr int QVRUB = 34;
static constexpr int QVCAN = 35;
static constexpr int QVHOME = 36;
static constexpr int QVEND = 37;
static constexpr int QVSPARE3 = 38;
static constexpr int QVSPARE4 = 39;

/*
 * Pre-Defined Baud rates used by cfgetospeed(), etc.
 */

static constexpr int QB0 = 0;
static constexpr int QB50 = 50;
static constexpr int QB75 = 75;
static constexpr int QB110 = 110;
static constexpr int QB134 = 134;
static constexpr int QB150 = 150;
static constexpr int QB200 = 200;
static constexpr int QB300 = 300;
static constexpr int QB600 = 600;
static constexpr int QB1200 = 1200;
static constexpr int QB1800 = 1800;
static constexpr int QB2400 = 2400;
static constexpr int QB4800 = 4800;
static constexpr int QB9600 = 9600;
static constexpr int QB19200 = 19200;
static constexpr int QEXTA = 19200;
static constexpr int QB38400 = 38400u;
static constexpr int QEXTB = 38400u;

static constexpr int QB57600 = 57600u;
static constexpr int QB76800 = 76800L;
static constexpr int QB115200 = 115200L;

/*
 * Optional Actions for tcsetattr()
 */

static constexpr int QTCSANOW = 0x0001;
static constexpr int QTCSADRAIN = 0x0002;
static constexpr int QTCSAFLUSH = 0x0004;

/*
 * queue_selectors for tcflush()
 */

static constexpr int QTCIFLUSH = 0x0000;
static constexpr int QTCOFLUSH = 0x0001;
static constexpr int QTCIOFLUSH = 0x0002;

/*
 * Actions for tcflow()
 */

static constexpr int QTCOOFF = 0x0000;
static constexpr int QTCOON = 0x0001;
static constexpr int QTCIOFF = 0x0002;
static constexpr int QTCION = 0x0003;

static constexpr int QTCOOFFHW = 0x0004;
static constexpr int QTCOONHW = 0x0005;
static constexpr int QTCIOFFHW = 0x0006;
static constexpr int QTCIONHW = 0x0007;

/*
 * QNX device status's (c_status) (read-only)
 */

static constexpr int QTC_ISFLOW = 0x0001;
static constexpr int QTC_OSFLOW = 0x0002;
static constexpr int QTC_IHFLOW = 0x0004;
static constexpr int QTC_OHFLOW = 0x0008;

/*
 * Qnx flags (c_qflag)
 */

static constexpr int QTC_PROTECT_HFLOW = 0x0001;
static constexpr int QTC_PROTECT_SFLOW = 0x0002;
static constexpr int QTC_WAIT_SFLOW = 0x0004;
static constexpr int QTC_NOPGRP = 0x0008;
static constexpr int QTC_ECHOI = 0x0010;
static constexpr int QTC_IFWD = 0x0020;
static constexpr int QTC_PROTECT_IEXTEN = 0x0040;


}
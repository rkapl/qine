#pragma once

#include <cstdint>
namespace Qnx {
static constexpr int QSIGHUP = 1;   /* hangup */
static constexpr int QSIGINT = 2;   /* interrupt */
static constexpr int QSIGQUIT = 3;   /* quit */
static constexpr int QSIGILL = 4;   /* illegal instruction */
static constexpr int QSIGTRAP = 5;   /* trace trap (not reset when caught) */
static constexpr int QSIGIOT = 6;   /* IOT instruction */
static constexpr int QSIGABRT = 6;   /* used by abort */
static constexpr int QSIGEMT = 7;   /* EMT instruction */
static constexpr int QSIGFPE = 8;   /* floating point exception */
static constexpr int QSIGKILL = 9;   /* kill (cannot be caught or ignored) */
static constexpr int QSIGBUS = 10;  /* bus error */
static constexpr int QSIGSEGV = 11;  /* segmentation violation */
static constexpr int QSIGSYS = 12;  /* bad argument to system call */
static constexpr int QSIGPIPE = 13;  /* write on pipe with no reader */
static constexpr int QSIGALRM = 14;  /* real-time alarm clock */
static constexpr int QSIGTERM = 15;  /* software termination signal from kill */
static constexpr int QSIGUSR1 = 16;  /* user defined signal 1 */
static constexpr int QSIGUSR2 = 17;  /* user defined signal 2 */
static constexpr int QSIGCHLD = 18;  /* death of child */
static constexpr int QSIGPWR = 19;  /* power-fail restart */
static constexpr int QSIGWINCH = 20;  /* window change */
static constexpr int QSIGURG = 21;  /* urgent condition on I/O channel */
static constexpr int QSIGPOLL = 22;  /* System V name for SIGIO */
static constexpr int QSIGIO = 22;  /* Asynchronus I/O */
static constexpr int QSIGSTOP = 23;  /* sendable stop signal not from tty */
static constexpr int QSIGTSTP = 24;  /* stop signal from tty */
static constexpr int QSIGCONT = 25;  /* continue a stopped process */
static constexpr int QSIGTTIN = 26;  /* attempted background tty read */
static constexpr int QSIGTTOU = 27;  /* attempted background tty write */
static constexpr int QSIGDEV = 28;  /* Dev event */

static constexpr int QSIGMAX = 32;
static constexpr int QSIGMIN = 1;

static constexpr uint32_t QSIG_ERR = UINT32_MAX;
static constexpr uint32_t QSIG_DFL = 0;
static constexpr uint32_t QSIG_IGN = 1;
static constexpr uint32_t QSIG_HOLD = 2;
}
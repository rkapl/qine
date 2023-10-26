#pragma once

namespace Qnx {

static constexpr int QTNOTIFY_SLEEP = 0;
static constexpr int QTNOTIFY_PROXY = 1;
static constexpr int QTNOTIFY_MESSENGER = 1;
static constexpr int QTNOTIFY_SIGNAL = 2;

static constexpr int QTIMER_ABSTIME = 0x0001;
static constexpr int QTIMER_ADDREL = 0x0100;
static constexpr int QTIMER_PRESERVE_EXEC = 0x0200;
static constexpr int QTIMER_AUTO_RELEASE = 0x0400;

}
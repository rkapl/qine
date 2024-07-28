#include "termios_settings.h"

TermiosSettings::TermiosSettings(int fd): m_fd(fd) {
    int r = tcgetattr(fd, &m_settings);
    m_ok = r == 0;
    m_old = m_settings;
}

TermiosSettings::~TermiosSettings() {
    if (m_ok) {
        tcsetattr(m_fd, TCSANOW, &m_old);
    }
}

void TermiosSettings::from_dev_read(const QnxMsg::dev::read_request& msg) {
    approximate(msg, &m_settings);
}

void TermiosSettings::approximate(const QnxMsg::dev::read_request& r, termios* dst)
{
    dst->c_lflag &= ~ICANON;
    if (r.m_timeout) {
        // ignore time and min and do timeout read
        // qnx can combine both, but we can't on Linux
        dst->c_cc[VTIME] = r.m_timeout;
        dst->c_cc[VMIN] = 0;
    } else {
        dst->c_cc[VTIME] = r.m_time;
        dst->c_cc[VMIN] = r.m_minimum;
    }
}

bool TermiosSettings::set() {
    return tcsetattr(m_fd, TCSANOW, &m_settings) == 0;
}
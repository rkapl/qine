#pragma once

#include <memory>
#include <unistd.h>

#include "cpp.h"

class UniqueFd: private NoCopy {
public:
    UniqueFd(): m_fd(-1) {}
    UniqueFd(int fd): m_fd(fd) {}
    int get() const { return m_fd; }
    inline UniqueFd(UniqueFd &&b);
    inline UniqueFd& operator=(UniqueFd &&b);
    inline bool valid() const;
    inline int release();
    inline ~UniqueFd();
private:
    int m_fd;
};

UniqueFd::UniqueFd(UniqueFd &&b): m_fd(b.release()) {
}

UniqueFd& UniqueFd::operator=(UniqueFd &&b)
{
    release();
    m_fd = b.release();
    return *this;
}

bool UniqueFd::valid() const {
    return ! (m_fd < 0);
}

int UniqueFd::release() {
    int old = m_fd;
    if (valid()) {
        close(m_fd);
        m_fd = -1;
    }
    return old;
}

UniqueFd::~UniqueFd() {
    release();
}
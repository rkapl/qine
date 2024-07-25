#pragma once

#include <memory>
#include <unistd.h>

#include "cpp.h"

// RAAI helper around file descriptor (host FD)
class UniqueFd: private NoCopy {
public:
    UniqueFd(): m_fd(-1) {}
    UniqueFd(int fd): m_fd(fd) {}
    int get() const { return m_fd; }
    inline UniqueFd(UniqueFd &&b);
    inline UniqueFd& operator=(UniqueFd &&b);
    inline bool valid() const;
    inline int release();
    inline void close();
    inline ~UniqueFd();

    static inline std::pair<UniqueFd, UniqueFd> create_pipe();
private:
    int m_fd;
};

UniqueFd::UniqueFd(UniqueFd &&b): m_fd(b.release()) {
}

UniqueFd& UniqueFd::operator=(UniqueFd &&b)
{
    close();
    m_fd = b.release();
    return *this;
}

bool UniqueFd::valid() const {
    return ! (m_fd < 0);
}

int UniqueFd::release() {
    int old = m_fd;
    m_fd = -1;
    return old;
}

void UniqueFd::close() {
    if (m_fd > 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

std::pair<UniqueFd, UniqueFd> UniqueFd::create_pipe() {
    int p[2];
    if (pipe(p) != 0) {
        return std::make_pair(UniqueFd(), UniqueFd());
    }
    return std::make_pair(UniqueFd(p[0]), UniqueFd(p[1]));
}

UniqueFd::~UniqueFd() {
    close();
}
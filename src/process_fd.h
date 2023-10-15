#pragma  once

#include <dirent.h>

/* Information for FD in a QNX process */
struct ProcessFd {
    ProcessFd():m_dir(nullptr) {}
    DIR *m_dir;
};
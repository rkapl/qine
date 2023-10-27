#include "fsutil.h"

#include <unistd.h>
#include <assert.h>
#include <string.h>

namespace Fsutil {

bool readlink(const char *path, std::string& dst) {
    dst.clear();
    
    size_t size = 256;
    int r;

    for (;;) {
        dst.resize(size + 1);
        r = ::readlink(path, dst.data(), dst.size());
        if (r < 0) {
            return false;
        } else if (r == size + 1) {
            size *= 2;
            continue;
        } else {
            dst.resize(size);
            return true;
        }
    }
}

bool ttyname(int host_fd, std::string& dst) {
    dst.clear();

    size_t size = 256;
    int r;

    for (;;) {
        dst.resize(size + 1);
        r = ::ttyname_r(host_fd, dst.data(), dst.size());
        // ttyname really stores the error in return, unlike anything else?
        if (r < 0) {
            if (r == ERANGE) {
                size *= 2;
                continue;
            } else {
                errno = r;
                return false;
            }
        } else {
            dst.resize(strlen(dst.c_str()));
            return true;
        }
    }
}

std::string getcwd() {
    return std::string(::get_current_dir_name());
}

std::string change_prefix(const char* prefix, const char *new_prefix, const char* path)
{
    assert(is_abs(prefix));
    assert(is_abs(new_prefix));

    std::string acc;

    // the path relative to both prefixes (without leading /, possibly empty)
    const char *common = path + strlen(prefix);
    while (*common == '/')
        common++;

    // construct the path
    acc.append(new_prefix);
    if (*path) {
        if (acc.back() != '/' ) {
            acc.push_back('/');
        }
        acc.append(common);
    }
    return acc;
}

}
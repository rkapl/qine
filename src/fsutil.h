#pragma once

#include <string>
#include "util.h"

namespace Fsutil {
    inline bool is_abs(const char *path) {
        return *path == '/';
    }

    inline bool is_root(const char *path) {
        return path[0] == '/' && path[1] == 0;
    }

    // Check if path starts with a given prefix. Expects two absolute canonical paths.
    inline bool path_starts_with(const char *path, const char *prefix) {
        return ::starts_with(path, prefix);
    }
    // Given path_starts_with(prefix, path), replace the prefix by new_prefix,
    std::string change_prefix(const char* prefix, const char *new_prefix, const char* path);

    bool readlink(const char *path, std::string& dst);
    std::string getcwd();
}
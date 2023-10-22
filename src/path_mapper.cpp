#include <string.h>
#include <assert.h>
#include "log.h"
#include "path_mapper.h"
#include "types.h"
#include "util.h"

static void pop_path(std::string& dst) {
    while (dst.back() != '/' && !dst.empty()) {
        dst.pop_back();
    }

    // pop separator unless we are root or empty
    if (dst.length() > 1) {
        dst.pop_back();
    }
}

static std::string normalize_path(const char *path) {
    std::string dst;
    // number of regular path segments that we can pop
    int segments = 0;

    if (*path == '/') {
        dst.push_back('/');
        path++;
    }

    // from this point dst is normalized path at all points
    while (*path) {
        const char *next_sep = strchr(path, '/');
        if (!next_sep)
            next_sep = path + strlen(path);
        size_t part_len = next_sep - path;
        
        if (strncmp(path, ".", part_len) == 0) {
            // just skip
        } else if (strncmp(path, "..", part_len) == 0) {
            if (segments > 0) {
                pop_path(dst);
                segments--;
            }
        } else {
            if (part_len > 0) {
                if (dst.back() != '/') {
                    dst.push_back('/');
                }
                dst.append(path, part_len);
                segments++;
            }
        }
        
        path = next_sep;
        if (*path == '/')
            path++;
    }

    return dst;
}


PathMapper::PathMapper() {
    m_root.m_host_path = "/";
    m_root.m_qnx_path = "/";
    m_root.m_exec_qine = true;
}

PathMapper::~PathMapper() {

}

PathInfo::PathInfo(): m_host_valid(false), m_qnx_valid(false), m_qnx_unmappable(false), m_prefix(NULL) {
}

PathInfo PathInfo::mk_qnx_path(const char *path, bool normalized) {
    PathInfo i;
    i.m_qnx_valid = true;
    i.m_qnx_path = path;
    if (!normalized) {
        i.m_qnx_path = normalize_path(path);
    } else {
        i.m_qnx_path = path;
    }
    return i;
}
PathInfo PathInfo::mk_host_path(const char *path, bool normalized) {
    PathInfo i;
    i.m_host_valid = true;
    if (!normalized) {
        i.m_host_path = normalize_path(path);
    } else {
        i.m_host_path = path;
    }
    return i;
}

void PathMapper::add_map(const char *map_cmd_arg) {
    std::string arg(map_cmd_arg);
    Prefix i;

    const char* separator = strchr(map_cmd_arg, ':');
    if (!separator) {
        throw ConfigurationError("map option expects source:target argument");
    }
    i.m_qnx_path.append(map_cmd_arg, separator - map_cmd_arg);
    i.m_qnx_path = normalize_path(i.m_qnx_path.c_str());
    i.m_host_path.append(separator + 1);
    i.m_host_path = normalize_path(i.m_host_path.c_str());

    if (i.m_qnx_path == "/") {
        i.m_exec_qine = true;
        m_root = i;
    } else {
        if (has_qnx_prefix(i.m_qnx_path.c_str())) {
            throw ConfigurationError("duplicate QNX prefix mapping");
        }
        i.m_exec_qine = false;
        m_prefixes.push_back(i);
    }
}
bool PathMapper::has_qnx_prefix(const std::string& pfx) {
    if (pfx == "/") {
        return true;
    }
    for (auto& pi: m_prefixes) {
        if (pi.m_qnx_path == pfx) {
            return true;
        }
    }
    return false;
}

void PathMapper::map_path_to_qnx(PathInfo &map) {
    map.m_qnx_valid = true;
    map.m_qnx_unmappable = true;
    map.m_prefix = NULL;
}

void PathMapper::map_path_to_host(PathInfo &map) {
    if (map.m_host_valid)
        return;
        
    assert(map.m_qnx_valid);
    Prefix *pfx = &m_root;

    // find the most specific prefix
    for (auto& c: m_prefixes) {
        if (starts_with(map.qnx_path(), c.m_qnx_path.c_str())) {
            if (c.m_qnx_path.length() > pfx->m_qnx_path.length())
                pfx = &c;
        }
    }

    map.m_host_valid = true;
    map.m_host_path.clear();
    map.m_host_path.append(pfx->m_host_path);
    map.m_host_path.push_back('/');
    map.m_host_path.append(map.m_qnx_path.substr(pfx->m_qnx_path.length()));
    Log::if_enabled(Log::MAP, [&](FILE *stream) {
        fprintf(stream, "Mapped qnx:%s -> host:%s (via %s)\n", map.qnx_path(), map.host_path(), pfx->m_qnx_path.c_str());
    });
}

PathInfo PathMapper::map_path_to_host(const char *path, bool normalized) {
    auto p = PathInfo::mk_qnx_path(path, normalized);
    map_path_to_host(p);
    return p;
}

/* Populate the info */
void PathMapper::map_path(PathInfo &map) {
    /* To get info, we need to do mapping, no matter where did the path come from */
    map_path_to_host(map);
    map_path_to_qnx(map);
}
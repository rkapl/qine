#include <string.h>
#include <assert.h>
#include "log.h"
#include "path_mapper.h"
#include "types.h"
#include "fsutil.h"
#include "cmd_opts.h"

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
            // add new part with normalization
            if (part_len > 0) {
                if ( !dst.empty() && dst.back() != '/') {
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
    m_root.m_exec = Exec::QNX;
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
    Prefix i;
    std::string exec_arg;
    CommandOptions::parse(map_cmd_arg, {
        .core = {
            new CommandOptions::String(&i.m_qnx_path),
            new CommandOptions::String(&i.m_host_path),
        },
        .kwargs {
            new CommandOptions::KwArg<CommandOptions::String>("exec", &exec_arg)
        }
    });

    i.m_qnx_path = normalize_path(i.m_qnx_path.c_str());
    i.m_host_path = normalize_path(i.m_host_path.c_str());

    if (exec_arg == "") {
        i.m_exec = i.m_qnx_path == "/" ? Exec::QNX : Exec::HOST;
    } else if (exec_arg == "qnx") {
        i.m_exec = Exec::QNX;
    } else if (exec_arg == "host") {
        i.m_exec = Exec::HOST;
    } else {
        throw ConfigurationError(std_printf("exec argument must be qnx or host"));
    }

    if (i.m_qnx_path == "/") {
        m_root = i;
    } else {
        if (has_qnx_prefix(i.m_qnx_path.c_str())) {
            throw ConfigurationError("duplicate QNX prefix mapping");
        }
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
    assert(map.m_host_valid);
    // TODO: if host path is not valid, readlink it from proc
    if (map.m_qnx_valid)
        return;

    Prefix *pfx = NULL;
    // find the most general QNX prefix
    for (auto& c: m_prefixes) {
        if (Fsutil::path_starts_with(map.host_path(), c.m_host_path.c_str())) {
            if (!pfx || c.m_qnx_path.length() < pfx->m_qnx_path.length())
                pfx = &c;
        }
    }
    if (Fsutil::path_starts_with(map.host_path(), m_root.m_host_path.c_str())) {
        pfx = &m_root;
    }

    if (pfx == nullptr) {
        map.m_qnx_valid = true;
        map.m_prefix = nullptr;
        map.m_qnx_unmappable = true;
        map.m_qnx_path.clear();
        map.m_qnx_path.append("/unmapped");
        map.m_qnx_path.append(map.m_host_path);
        Log::if_enabled(Log::MAP, [&](FILE *stream) {
            fprintf(stream, "Unmappable host:%s\n", map.host_path());
        });
        return;
    }

    map.m_qnx_valid = true;
    map.m_qnx_unmappable = false;
    map.m_qnx_path.clear();
    map.m_qnx_path = Fsutil::change_prefix(pfx->m_host_path.c_str(), pfx->m_qnx_path.c_str(), map.m_host_path.c_str());
    map.m_prefix = nullptr;

    Log::if_enabled(Log::MAP, [&](FILE *stream) {
        fprintf(stream, "Mapped host:%s -> qnx:%s (via %s)\n", map.host_path(), map.qnx_path(), pfx->m_qnx_path.c_str());
    });

    map.m_qnx_valid = true;

    map.m_qnx_unmappable = true;
    map.m_prefix = NULL;
}

PathInfo PathMapper::map_path_to_qnx(const char *path, bool normalized)
{
    auto p = PathInfo::mk_host_path(path, normalized);
    map_path_to_qnx(p);
    return p;
}

void PathMapper::map_path_to_host(PathInfo &map) {
    assert(map.m_qnx_valid);
    if (map.m_host_valid)
        return;
        
    assert(map.m_qnx_valid);
    Prefix *pfx = &m_root;

    // find the most specific prefix
    for (auto& c: m_prefixes) {
        if (Fsutil::path_starts_with(map.qnx_path(), c.m_qnx_path.c_str())) {
            if (c.m_qnx_path.length() > pfx->m_qnx_path.length())
                pfx = &c;
        }
    }

    map.m_host_valid = true;
    map.m_host_path.clear();
    map.m_host_path = Fsutil::change_prefix(pfx->m_qnx_path.c_str(), pfx->m_host_path.c_str(), map.m_qnx_path.c_str());
    map.m_prefix = pfx;
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
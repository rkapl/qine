#pragma once

#include <string>
#include <vector>

class PathInfo;

class PathMapper {
friend class PathInfo;
public:
    PathMapper();
    ~PathMapper();

    // configuration
    void add_map(const char *map_cmd_arg);

    // runtime

    /* Populate the m_qnx_path mapping in path info. Cannot fail, but will produce /unmapped/* path and set a flag.. */
    void map_path_to_qnx(PathInfo &map);
    PathInfo map_path_to_qnx(const char *path, bool normalized=false);

    /* Populate the m_host_path mapping in path info. Cannot fail */
    void map_path_to_host(PathInfo &map);
    PathInfo map_path_to_host(const char *path, bool normalized=false);

    /* Populate the info. May fail. */
    void map_path(PathInfo &map);
private:
    struct Prefix {
        std::string m_host_path;
        std::string m_qnx_path;
        bool m_exec_qine;
    };
    bool has_qnx_prefix(const std::string& pfx);
    Prefix m_root;
    std::vector<Prefix> m_prefixes;
};

/* Information about paths. They can either start as host or qnx paths and then be subsequently resolved. */
class PathInfo {
friend class PathMapper;
public:
    PathInfo();
    static PathInfo mk_qnx_path(const char *path, bool normalized=false);
    static PathInfo mk_host_path(const char *path, bool normalized=false);

    bool host_valid() const { return m_host_valid; }
    const char *host_path() const {return m_host_path.c_str(); }

    bool qnx_valid() const { return m_qnx_valid; }
    const char *qnx_path() const {return m_qnx_path.c_str(); }

    bool info_valid() const {return m_prefix; }
    const char* symlink_root() const {return m_prefix->m_host_path.c_str(); }
    bool exec_qine() const {return m_prefix->m_exec_qine; }
private:
    bool m_host_valid;
    std::string m_host_path;

    bool m_qnx_valid;
    bool m_qnx_unmappable;
    std::string m_qnx_path;

    PathMapper::Prefix* m_prefix;
};
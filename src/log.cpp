#include "log.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdarg.h>

uint64_t Log::m_enabled =  (Log::log_mask(Category::UNHANDLED));

void Log::print(Category c, const char *format, ...)  
{
    va_list v;
    va_start(v, format);
    if_enabled(c, [format, &v] (FILE* out) {
        vfprintf(out, format, v);
    });
    va_end(v);
}

void Log::enable(Category c, bool enabled) {
    m_enabled &= ~log_mask(c);
    if (enabled) {
        m_enabled |= log_mask(c);
    }
}

Log::Category Log::by_name(const char *name) {
    if (strcmp(name, "msg") == 0) {
        return MSG;
    }
    if (strcmp(name, "msg_reply") == 0) {
        return MSG_REPLY;
    }
    if (strcmp(name, "unhandled") == 0) {
        return UNHANDLED;
    }
    if (strcmp(name, "loader") == 0) {
        return LOADER;
    }
    if (strcmp(name, "sig") == 0) {
        return SIG;
    }
    if (strcmp(name, "map") == 0) {
        return MAP;
    }
    if (strcmp(name, "fd") == 0) {
        return FD;
    }
    return Category::INVALID;
}
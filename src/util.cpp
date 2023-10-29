#include "util.h"
#include <stdexcept>

bool starts_with(std::string_view string, std::string_view prefix) {
    if (prefix.length() > string.length())
        return false;

    return prefix == string.substr(0, prefix.length());
}

std::string std_printf(const char *format, ...) {
    va_list va;
    va_start(va, format);
    auto s = std_vprintf(format, va);
    va_end(va);
    return s;
}

std::string std_vprintf(const char *format, va_list args) {
    va_list args_tmp;
    va_copy(args_tmp, args);
    auto req = vsnprintf(nullptr, 0, format, args_tmp);
    if (req < 0)
        throw std::logic_error("vsprintf returned an errro");
    std::string buf;
    buf.resize(req + 1);
    va_copy(args_tmp, args_tmp);
    vsnprintf(buf.data(), req + 1, format, args);
    buf.resize(req);
    return buf;
}
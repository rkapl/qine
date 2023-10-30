#pragma once

#include <string_view>
#include <stdarg.h>
#include <string>

// Check if string starts with a given prefix.
bool starts_with(std::string_view string, std::string_view prefix);

std::string std_printf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
std::string std_vprintf(const char *format, va_list args) __attribute__ ((format (printf, 1, 0)));
size_t qine_strlcpy(char * dst, const char * src, size_t maxlen);

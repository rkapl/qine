#include "util.h"

bool starts_with(std::string_view string, std::string_view prefix) {
    if (prefix.length() > string.length())
        return false;

    return prefix == string.substr(0, prefix.length());
}
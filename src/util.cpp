#include "util.h"

bool starts_with(const char *string, const char *prefix) {
    while (*prefix) {
        if (*prefix != *string)
            return false;

        prefix++; string++;
    }
    return true;
}
#pragma once

#include <cstdint>
#include <stdint.h>
#include <stdio.h>



class Log {
public:
    enum Category {
        INVALID,
        MSG,
        MSG_REPLY,
        LOADER,
        UNHANDLED,
        MAP,
        FD,
        SIG,
    };

    static void print(Category c, const char *format, ...) __attribute__((format(printf, 2, 3)));
    static void enable(Category c, bool enabled);
    static Category by_name(const char *name);

    /* Call functor F with dst FILE stream if selected log category is enabled */
    template<class F>
    static void if_enabled(Category c, F logf) {
        if (log_mask(c) & m_enabled) {
            logf(stderr);
        }
    }
private:
    static constexpr uint64_t log_mask(Category c) {
        return (static_cast<uint64_t>(1) << (int) c);
    }
    static uint64_t m_enabled;
};

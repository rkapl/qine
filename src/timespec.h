#pragma  once

#include <stdio.h>
#include <time.h>
#include <stdint.h>

/**
 * Wrapper around timespec
 *
 * @remark Negative value not yet tested
 */
struct Timespec: timespec {
    Timespec() = default;
    Timespec(int64_t sec, int32_t nsec) {
        tv_sec = sec;
        tv_nsec = nsec;
        // normalize

        tv_sec += tv_nsec / (1000 * 1000 * 1000);
        tv_nsec %= 1000 * 1000 * 1000;

        if (tv_sec > 0 && tv_nsec < 0) {
            tv_sec--;
            tv_nsec += 1000 * 1000 * 1000;
        }
    }

    static const Timespec ZERO;

    static Timespec ms(int64_t ms) {
        return Timespec(ms / 1000, (ms % 1000) * 1000 * 1000);
    }

    Timespec operator+(const Timespec &b) const {
        return Timespec(tv_sec + b.tv_sec, tv_nsec + b.tv_nsec);
    }

    bool operator>(const Timespec &b) const {
        return tv_sec > b.tv_sec || (tv_sec == b.tv_sec && tv_nsec > b.tv_nsec);
    }

    bool operator>=(const Timespec &b) const {
        return tv_sec > b.tv_sec || (tv_sec == b.tv_sec && tv_nsec >= b.tv_nsec);
    }

    Timespec operator-(const Timespec &b) const {
        return Timespec(tv_sec - b.tv_sec, tv_nsec - b.tv_nsec);
    }
};

/** An absolute timespec or no deadline at all */
struct Deadline {
    Deadline(): v(0, 0) {}
    Deadline(const Timespec& ts): v(ts) {
    }
    operator bool() const { return v.tv_sec != 0 || v.tv_nsec != 0;}
    bool is_set() const {return *this;}
    /** Combine with another deadline to preserve the strictes (nearest) one */
    void operator|=(const Deadline &b) {
        if (!this->is_set() || (b.is_set() && b < *this)) {
            *this = b;
        }
    }
    Timespec v;
};
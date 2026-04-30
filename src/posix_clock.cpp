#include "sap_core/clock.h"

#include <string.h>
#include <time.h>

namespace sap::core {

    system_monotonic_clock::system_monotonic_clock() : m_frequency_hz(0), m_start_ticks(0) {}

    stl::result<i64> system_monotonic_clock::now_ns() const {
        struct timespec ts;
        int res = ::clock_gettime(CLOCK_MONOTONIC, &ts);
        if (res == -1) {
            return stl::make_error<i64>("SystemMonotonicClock::now_ns: {}", ::strerror(errno));
        }
        return (i64)ts.tv_sec * 1'000'000'000 + (i64)ts.tv_nsec;
    }
} // namespace mdcap::core

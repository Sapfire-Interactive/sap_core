#include "sap_core/clock.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace sap::core {

    system_monotonic_clock::system_monotonic_clock() : m_frequency_hz(0), m_start_ticks(0) {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        m_frequency_hz = freq.QuadPart;
    }

    stl::result<i64> system_monotonic_clock::now_ns() const {
        LARGE_INTEGER now;
        if (!QueryPerformanceCounter(&now))
            return stl::make_error<i64>("system_monotonic_clock::now_ns: QueryPerformanceCounter failed");
        // Absolute counter since boot, like CLOCK_MONOTONIC; split to avoid overflow.
        i64 secs = now.QuadPart / m_frequency_hz;
        i64 rem = now.QuadPart % m_frequency_hz;
        return secs * 1'000'000'000 + (rem * 1'000'000'000) / m_frequency_hz;
    }
} // namespace sap::core

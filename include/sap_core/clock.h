#pragma once
#include <concepts>
#include <sap_core/stl/result.h>
#include <sap_core/types.h>

namespace sap::core {

    template <typename S>
    concept monotonic_clock = requires(S& s) {
        { s.now_ns() } -> std::convertible_to<stl::result<i64>>;
    };

    // Default: clock_gettime(CLOCK_MONOTONIC) on Linux,
    // QueryPerformanceCounter on Windows. See §8 for the Windows conversion.
    class system_monotonic_clock {
    public:
        system_monotonic_clock();
        stl::result<i64> now_ns() const;

    private:
        i64 m_frequency_hz; // Windows only; unused on Linux.
        i64 m_start_ticks; // Windows only; unused on Linux.
    };
    static_assert(monotonic_clock<system_monotonic_clock>);

    // For tests: caller drives time manually.
    class fake_clock {
    public:
        stl::result<i64> now_ns() const { return m_now; }
        void advance_ns(i64 delta) { m_now += delta; }

    private:
        i64 m_now = 0;
    };
    static_assert(monotonic_clock<fake_clock>);

} // namespace mdcap::core

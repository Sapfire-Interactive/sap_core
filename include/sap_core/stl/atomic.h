#pragma once

#include <atomic>

namespace stl {
    template <typename T>
    using atomic = std::atomic<T>;

    using memory_order = std::memory_order;
    constexpr auto memory_order_relaxed = std::memory_order_relaxed;
    constexpr auto memory_order_consume = std::memory_order_consume;
    constexpr auto memory_order_acquire = std::memory_order_acquire;
    constexpr auto memory_order_release = std::memory_order_release;
    constexpr auto memory_order_acq_rel = std::memory_order_acq_rel;
    constexpr auto memory_order_seq_cst = std::memory_order_seq_cst;
} // namespace stl

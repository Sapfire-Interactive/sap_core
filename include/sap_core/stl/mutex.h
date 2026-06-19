#pragma once

#include <mutex>

namespace stl {
    using mutex = std::mutex;
    using recursive_mutex = std::recursive_mutex;

    template <typename Mutex>
    using lock_guard = std::lock_guard<Mutex>;

    template <typename Mutex>
    using unique_lock = std::unique_lock<Mutex>;

    template <typename Mutex>
    using scoped_lock = std::scoped_lock<Mutex>;
} // namespace stl

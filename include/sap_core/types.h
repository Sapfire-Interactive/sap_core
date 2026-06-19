#pragma once

#include "sap_core/stl/array.h"
#include "sap_core/stl/atomic.h"
#include "sap_core/stl/bitset.h"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>

#include "sap_core/stl/string_view.h"
#include <utility>

#include "sap_core/stl/queue.h"
#include "sap_core/stl/span.h"

#include "sap_core/stl/tuple.h"

#include <cstdint>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

namespace stl {

    template <class T>
    using reference_wrapper = std::reference_wrapper<T>;

    template <typename T>
    using function = std::function<T>;

    using mutex = std::mutex;
    using recursive_mutex = std::recursive_mutex;

    template <typename Mutex>
    using lock_guard = std::lock_guard<Mutex>;

    template <typename Mutex>
    using unique_lock = std::unique_lock<Mutex>;

    template <typename Mutex>
    using scoped_lock = std::scoped_lock<Mutex>;

    using condition_variable = std::condition_variable;

    using thread = std::thread;
    using jthread = std::jthread;
    using stop_token = std::stop_token;

    namespace this_thread {
        using std::this_thread::sleep_for;
        using std::this_thread::sleep_until;
        using std::this_thread::get_id;
        using std::this_thread::yield;
    }

    using condition_variable_any = std::condition_variable_any;
    using condition_variable = std::condition_variable;

    template <typename T>
    using optional = std::optional<T>;

    using byte = std::byte;

    using size_t = std::size_t;

    // Memory size helpers
    constexpr inline u64 gibibytes(u32 amount) { return static_cast<u64>(amount) * 1024ULL * 1024ULL * 1024ULL; }
    constexpr inline u64 mebibytes(u32 amount) { return static_cast<u64>(amount) * 1024ULL * 1024ULL; }
    constexpr inline u64 kibibytes(u32 amount) { return static_cast<u64>(amount) * 1024ULL; }
    constexpr inline u64 gigabytes(u32 amount) { return static_cast<u64>(amount) * 1000ULL * 1000ULL * 1000ULL; }
    constexpr inline u64 megabytes(u32 amount) { return static_cast<u64>(amount) * 1000ULL * 1000ULL; }
    constexpr inline u64 kilobytes(u32 amount) { return static_cast<u64>(amount) * 1000ULL; }

} // namespace stl

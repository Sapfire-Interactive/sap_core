#pragma once

#include <array>
#include <bitset>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <span>
#include <sstream>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>

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

    // Non-allocating STL type aliases
    template <class... Ts>
    using tuple = std::tuple<Ts...>;

    template <class T>
    using reference_wrapper = std::reference_wrapper<T>;

    template <typename T, class Container = std::deque<T>>
    using queue = std::queue<T, Container>;

    template <typename T>
    using function = std::function<T>;

    using string_view = std::string_view;
    using wstring_view = std::wstring_view;

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

    template <typename T>
    using optional = std::optional<T>;

    template <typename T, std::size_t Extent = std::dynamic_extent>
    using span = std::span<T, Extent>;

    template <class T, size_t N>
    using array = std::array<T, N>;

    template <size_t N>
    using bitset = std::bitset<N>;

    using byte = std::byte;

    template <class T, std::size_t Extent>
    constexpr auto as_bytes(std::span<T, Extent> s) noexcept {
        return std::as_bytes(s);
    }

    template <class T, std::size_t Extent>
    constexpr auto as_writable_bytes(std::span<T, Extent> s) noexcept {
        return std::as_writable_bytes(s);
    }

    template <typename T>
    using atomic = std::atomic<T>;

    using size_t = std::size_t;

    // Memory size helpers
    constexpr inline u64 gibibytes(u32 amount) { return static_cast<u64>(amount) * 1024ULL * 1024ULL * 1024ULL; }
    constexpr inline u64 mebibytes(u32 amount) { return static_cast<u64>(amount) * 1024ULL * 1024ULL; }
    constexpr inline u64 kibibytes(u32 amount) { return static_cast<u64>(amount) * 1024ULL; }
    constexpr inline u64 gigabytes(u32 amount) { return static_cast<u64>(amount) * 1000ULL * 1000ULL * 1000ULL; }
    constexpr inline u64 megabytes(u32 amount) { return static_cast<u64>(amount) * 1000ULL * 1000ULL; }
    constexpr inline u64 kilobytes(u32 amount) { return static_cast<u64>(amount) * 1000ULL; }

} // namespace stl

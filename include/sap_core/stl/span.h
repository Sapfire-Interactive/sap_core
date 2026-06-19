#pragma once

#include <cstddef>
#include <span>

namespace stl {
    template <typename T, std::size_t Extent = std::dynamic_extent>
    using span = std::span<T, Extent>;

    template <class T, std::size_t Extent>
    constexpr auto as_bytes(std::span<T, Extent> s) noexcept {
        return std::as_bytes(s);
    }

    template <class T, std::size_t Extent>
    constexpr auto as_writable_bytes(std::span<T, Extent> s) noexcept {
        return std::as_writable_bytes(s);
    }
} // namespace stl

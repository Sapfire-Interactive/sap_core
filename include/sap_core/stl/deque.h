#pragma once

#include <deque>

namespace stl {
    template <typename T, class Allocator = std::allocator<T>>
    using deque = std::deque<T, Allocator>;
} // namespace stl

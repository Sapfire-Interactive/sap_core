#pragma once

#include <deque>
#include <queue>

namespace stl {
    template <typename T, class Container = std::deque<T>>
    using queue = std::queue<T, Container>;
} // namespace stl

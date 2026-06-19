#pragma once

#include <functional>

namespace stl {
    template <class T>
    using reference_wrapper = std::reference_wrapper<T>;

    template <typename T>
    using function = std::function<T>;
} // namespace stl

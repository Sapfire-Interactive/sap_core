#pragma once

#include <tuple>

namespace stl {
    template <class... Ts>
    using tuple = std::tuple<Ts...>;
} // namespace stl

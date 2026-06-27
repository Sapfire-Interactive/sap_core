#pragma once

#include <chrono>

namespace stl::chrono {
    using std::chrono::duration;
    using std::chrono::duration_cast;
    using std::chrono::hours;
    using std::chrono::milliseconds;
    using std::chrono::minutes;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;
    using std::chrono::steady_clock;
    using std::chrono::system_clock;
    using std::chrono::time_point;

    namespace literals {
        using namespace std::chrono_literals;
    }
} // namespace stl::chrono

namespace stl {
    namespace chrono_literals = stl::chrono::literals;
}

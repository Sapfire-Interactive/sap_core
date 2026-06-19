#pragma once

#include <thread>

namespace stl {
    using thread = std::thread;
    using jthread = std::jthread;
    using stop_token = std::stop_token;

    namespace this_thread {
        using std::this_thread::get_id;
        using std::this_thread::sleep_for;
        using std::this_thread::sleep_until;
        using std::this_thread::yield;
    }
} // namespace stl

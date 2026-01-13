#pragma once

#include "types.h"
#include <chrono>

using Timestamp = i64;  // Milliseconds since Unix epoch

inline Timestamp now_ms() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}
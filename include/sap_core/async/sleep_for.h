#pragma once

#include "sap_core/async/executor.h"
#include "sap_core/async/task.h"

#include <chrono>

namespace sap::async {

    // Linux only currently; the Windows path is a stub that returns
    // immediately. Verify a real timer integration before relying on it
    // for cross-platform timing.
    Task<void> sleep_for(Executor& ex, std::chrono::milliseconds dt);

} // namespace sap::async

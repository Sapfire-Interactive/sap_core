#pragma once

#include "sap_core/async/executor.h"
#include "sap_core/async/stop_token.h"
#include "sap_core/async/task.h"

#include <chrono>

namespace sap::async {

    // Linux: timerfd-backed. Windows: stub that returns immediately.
    Task<void> sleep_for(Executor& ex, std::chrono::milliseconds dt, StopToken tok = {});

} // namespace sap::async

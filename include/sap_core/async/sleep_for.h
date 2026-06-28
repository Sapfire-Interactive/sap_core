#pragma once

#include "sap_core/async/executor.h"
#include "sap_core/async/stop_token.h"
#include "sap_core/async/task.h"
#include "sap_core/platform.h"
#include "sap_core/stl/result.h"

#include <chrono>

namespace sap::async {

    // Linux: timerfd-backed. Windows: loopback socket woken by a threadpool timer.
    SAP_CORE_API Task<stl::result<>> sleep_for(Executor& ex, std::chrono::milliseconds dt, StopToken tok = {});

} // namespace sap::async

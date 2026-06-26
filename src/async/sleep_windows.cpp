#include "sap_core/async/sleep_for.h"

#include "sap_core/async/executor.h"
#include "sap_core/async/task.h"

#include <chrono>

namespace sap::async {

    // TODO: real Windows timer integration. Options worth evaluating once
    // we're on a Windows machine:
    //   - CreateWaitableTimer + RegisterWaitForSingleObject completion bound
    //     to the IOCP via PostQueuedCompletionStatus.
    //   - A userspace timer wheel polled at the top of each Executor loop
    //     iteration, woken via reactor.wake().
    // Returns immediately so callers compile cleanly; do not rely on it
    // until replaced.
    Task<stl::result<>> sleep_for(Executor&, std::chrono::milliseconds, StopToken) { co_return stl::success; }

} // namespace sap::async

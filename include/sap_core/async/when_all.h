#pragma once

#include "sap_core/async/task.h"
#include "sap_core/stl/tuple.h"
#include "sap_core/stl/utility.h"

namespace sap::async {

    // Awaits tasks in argument order; void Ts unsupported.
    template <typename... Ts>
    Task<stl::tuple<Ts...>> when_all(Task<Ts>... tasks) {
        co_return stl::tuple<Ts...>{(co_await stl::move(tasks))...};
    }

} // namespace sap::async

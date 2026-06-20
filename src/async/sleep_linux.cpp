#include "sap_core/async/sleep_for.h"

#include "sap_core/async/executor.h"
#include "sap_core/async/task.h"
#include "sap_core/io/event.h"
#include "sap_core/types.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <ctime>

namespace sap::async {

    Task<void> sleep_for(Executor& ex, std::chrono::milliseconds dt) {
        int tfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
        if (tfd < 0)
            co_return;

        ::itimerspec spec{};
        spec.it_value.tv_sec  = static_cast<time_t>(dt.count() / 1000);
        spec.it_value.tv_nsec = static_cast<long>((dt.count() % 1000) * 1'000'000);
        // A zero deadline means "disarm", not "fire now"; force at least 1ns.
        if (spec.it_value.tv_sec == 0 && spec.it_value.tv_nsec == 0)
            spec.it_value.tv_nsec = 1;

        if (::timerfd_settime(tfd, 0, &spec, nullptr) < 0) {
            ::close(tfd);
            co_return;
        }

        IoAwaiter awaiter(ex, tfd, sap::io::Event::Readable);
        co_await awaiter;

        u64 expirations = 0;
        (void)::read(tfd, &expirations, sizeof(expirations));
        ::close(tfd);
    }

} // namespace sap::async

#include "sap_core/async/sleep_for.h"

#include "sap_core/async/executor.h"
#include "sap_core/async/stop_token.h"
#include "sap_core/async/task.h"
#include "sap_core/io/event.h"
#include "sap_core/stl/utility.h"
#include "sap_core/types.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <ctime>

namespace sap::async {

    namespace {

        struct fd_guard {
            int fd;
            explicit fd_guard(int f) noexcept : fd(f) {}
            fd_guard(const fd_guard&)            = delete;
            fd_guard& operator=(const fd_guard&) = delete;
            fd_guard(fd_guard&& o) noexcept : fd(stl::exchange(o.fd, -1)) {}
            ~fd_guard() {
                if (fd >= 0)
                    ::close(fd);
            }
        };

    } // namespace

    Task<stl::result<>> sleep_for(Executor& ex, std::chrono::milliseconds dt, StopToken tok) {
        fd_guard tfd{::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK)};
        if (tfd.fd < 0)
            co_return stl::make_error<>("sleep_for: timerfd_create failed: {}", errno);

        ::itimerspec spec{};
        spec.it_value.tv_sec  = static_cast<time_t>(dt.count() / 1000);
        spec.it_value.tv_nsec = static_cast<long>((dt.count() % 1000) * 1'000'000);
        if (spec.it_value.tv_sec == 0 && spec.it_value.tv_nsec == 0)
            spec.it_value.tv_nsec = 1;

        if (::timerfd_settime(tfd.fd, 0, &spec, nullptr) < 0)
            co_return stl::make_error<>("sleep_for: timerfd_settime failed: {}", errno);

        IoAwaiter awaiter(ex, tfd.fd, sap::io::Event::Readable, stl::move(tok));
        if (auto r = co_await awaiter; !r)
            co_return r;

        u64 expirations = 0;
        (void)::read(tfd.fd, &expirations, sizeof(expirations));
        co_return stl::success;
    }

} // namespace sap::async

#pragma once

#include "sap_core/async/task.h"
#include "sap_core/stl/coroutine.h"
#include "sap_core/stl/exception.h"
#include "sap_core/stl/optional.h"
#include "sap_core/stl/semaphore.h"
#include "sap_core/stl/utility.h"

#include <type_traits>

namespace sap::async {

    namespace detail {

        // Private task type used by sync_wait. Its final_suspend signals a
        // semaphore so the blocking call can return.
        template <typename T>
        struct sync_wait_task;

        struct sync_wait_promise_base {
            stl::binary_semaphore* sem = nullptr;
            stl::exception_ptr exc;

            stl::suspend_always initial_suspend() noexcept { return {}; }

            struct final_awaiter {
                bool await_ready() noexcept { return false; }
                template <typename Promise>
                void await_suspend(stl::coroutine_handle<Promise> h) noexcept {
                    h.promise().sem->release();
                }
                void await_resume() noexcept {}
            };

            final_awaiter final_suspend() noexcept { return {}; }
            void unhandled_exception() { exc = stl::current_exception(); }
        };

        template <typename T>
        struct sync_wait_promise : sync_wait_promise_base {
            stl::optional<T> value;
            sync_wait_task<T> get_return_object();
            template <typename U>
            void return_value(U&& v) {
                value.emplace(stl::forward<U>(v));
            }
        };

        template <>
        struct sync_wait_promise<void> : sync_wait_promise_base {
            sync_wait_task<void> get_return_object();
            void return_void() {}
        };

        template <typename T>
        struct sync_wait_task {
            using promise_type = sync_wait_promise<T>;
            stl::coroutine_handle<promise_type> handle;

            explicit sync_wait_task(stl::coroutine_handle<promise_type> h) noexcept : handle(h) {}
            sync_wait_task(sync_wait_task&& o) noexcept : handle(stl::exchange(o.handle, {})) {}
            ~sync_wait_task() {
                if (handle)
                    handle.destroy();
            }

            void start(stl::binary_semaphore& s) {
                handle.promise().sem = &s;
                handle.resume();
            }
        };

        template <typename T>
        sync_wait_task<T> sync_wait_promise<T>::get_return_object() {
            return sync_wait_task<T>(stl::coroutine_handle<sync_wait_promise<T>>::from_promise(*this));
        }

        inline sync_wait_task<void> sync_wait_promise<void>::get_return_object() {
            return sync_wait_task<void>(stl::coroutine_handle<sync_wait_promise<void>>::from_promise(*this));
        }

        template <typename T>
        sync_wait_task<T> make_sync_wait_task(Task<T>& task) {
            if constexpr (std::is_void_v<T>) {
                co_await stl::move(task);
            } else {
                co_return co_await stl::move(task);
            }
        }

    } // namespace detail

    // Run `task` to completion, blocking the calling thread. Returns the task's
    // value (or void). Rethrows any exception the task propagated.
    //
    // Round 1: only useful for tasks that complete without external resumption,
    // since there's no reactor yet — if the task suspends waiting on I/O nothing
    // will resume it and this call hangs.
    template <typename T>
    T sync_wait(Task<T> task) {
        stl::binary_semaphore sem{0};
        auto runner = detail::make_sync_wait_task(task);
        runner.start(sem);
        sem.acquire();
        if (runner.handle.promise().exc)
            stl::rethrow_exception(runner.handle.promise().exc);
        if constexpr (!std::is_void_v<T>) {
            return stl::move(*runner.handle.promise().value);
        }
    }

} // namespace sap::async

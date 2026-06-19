#pragma once

#include "sap_core/stl/coroutine.h"
#include "sap_core/stl/exception.h"
#include "sap_core/stl/optional.h"
#include "sap_core/stl/utility.h"

#include <type_traits>

namespace sap::async {

    template <typename T>
    class Task;

    namespace detail {

        // Value/exception storage split between T and void specialization so we
        // can use return_value/return_void without if-constexpr in the promise.
        template <typename T>
        struct task_value_storage {
            stl::optional<T> value;

            template <typename U>
            void return_value(U&& v) {
                value.emplace(stl::forward<U>(v));
            }
            T take() {
                return stl::move(*value);
            }
        };

        template <>
        struct task_value_storage<void> {
            void return_void() {}
            void take() {}
        };

        struct task_promise_base {
            stl::exception_ptr exc;
            stl::coroutine_handle<> continuation;
            // Set by Task::detach(); flips final_suspend to self-destroy
            // the frame instead of transferring to a continuation.
            bool detached = false;

            stl::suspend_always initial_suspend() noexcept { return {}; }

            struct final_awaiter {
                bool await_ready() noexcept { return false; }

                // Symmetric transfer back to whoever awaited us — or noop_coroutine
                // if detached / no awaiter — so chained co_awaits don't grow the stack.
                template <typename Promise>
                stl::coroutine_handle<> await_suspend(stl::coroutine_handle<Promise> h) noexcept {
                    auto& p = h.promise();
                    if (p.detached) {
                        h.destroy();
                        return stl::noop_coroutine();
                    }
                    return p.continuation ? p.continuation : stl::noop_coroutine();
                }

                void await_resume() noexcept {}
            };

            final_awaiter final_suspend() noexcept { return {}; }
            void unhandled_exception() { exc = stl::current_exception(); }
        };

    } // namespace detail

    // Lazy coroutine task. Default usage: `co_await task` or `sync_wait(task)`.
    // Call `.detach()` to release ownership and let the coroutine run to
    // completion autonomously (the frame self-destroys at final_suspend).
    template <typename T>
    class Task {
    public:
        using value_type = T;

        struct promise_type
            : detail::task_promise_base
            , detail::task_value_storage<T> {
            Task get_return_object() {
                return Task(stl::coroutine_handle<promise_type>::from_promise(*this));
            }
        };

        using handle_type = stl::coroutine_handle<promise_type>;

        Task() noexcept = default;
        explicit Task(handle_type h) noexcept : m_handle(h) {}

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        Task(Task&& other) noexcept : m_handle(stl::exchange(other.m_handle, {})) {}

        Task& operator=(Task&& other) noexcept {
            if (this != &other) {
                if (m_handle)
                    m_handle.destroy();
                m_handle = stl::exchange(other.m_handle, {});
            }
            return *this;
        }

        ~Task() {
            if (m_handle)
                m_handle.destroy();
        }

        bool await_ready() const noexcept { return !m_handle || m_handle.done(); }

        // Symmetric transfer: jump straight into our coroutine instead of returning,
        // so deeply nested co_awaits don't accumulate stack frames.
        handle_type await_suspend(stl::coroutine_handle<> awaiter) noexcept {
            m_handle.promise().continuation = awaiter;
            return m_handle;
        }

        T await_resume() {
            auto& p = m_handle.promise();
            if (p.exc)
                stl::rethrow_exception(p.exc);
            return p.take();
        }

        // Release ownership of the frame and start the body. The coroutine runs
        // to completion on its own; the frame is freed at final_suspend. Awaiting
        // a detached task is a no-op (and `done()` returns true immediately).
        void detach() {
            if (!m_handle)
                return;
            m_handle.promise().detached = true;
            auto h = stl::exchange(m_handle, {});
            h.resume();
        }

        bool done() const noexcept { return !m_handle || m_handle.done(); }
        explicit operator bool() const noexcept { return static_cast<bool>(m_handle); }

        handle_type handle() const noexcept { return m_handle; }

    private:
        handle_type m_handle{};
    };

} // namespace sap::async

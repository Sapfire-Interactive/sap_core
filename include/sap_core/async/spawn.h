#pragma once

#include "sap_core/async/executor.h"
#include "sap_core/async/task.h"
#include "sap_core/stl/coroutine.h"
#include "sap_core/stl/exception.h"
#include "sap_core/stl/optional.h"
#include "sap_core/stl/utility.h"

#include <type_traits>

namespace sap::async {

    template <typename T>
    class SpawnHandle;

    namespace detail {

        template <typename T>
        struct spawn_value_storage {
            stl::optional<T> value;

            template <typename U>
            void return_value(U&& v) {
                value.emplace(stl::forward<U>(v));
            }
            T take() { return stl::move(*value); }
        };

        template <>
        struct spawn_value_storage<void> {
            void return_void() {}
            void take() {}
        };

        struct spawn_promise_base {
            stl::exception_ptr      exc;
            stl::coroutine_handle<> continuation;
            // Set by ~SpawnHandle when the runner is still pending; tells final_suspend
            // to self-destroy instead of transferring to a continuation that no longer exists.
            bool                    handle_dropped = false;

            stl::suspend_always initial_suspend() noexcept { return {}; }

            struct final_awaiter {
                bool await_ready() noexcept { return false; }
                template <typename Promise>
                stl::coroutine_handle<> await_suspend(stl::coroutine_handle<Promise> h) noexcept {
                    auto& p = h.promise();
                    if (p.handle_dropped) {
                        h.destroy();
                        return stl::noop_coroutine();
                    }
                    return p.continuation ? p.continuation : stl::noop_coroutine();
                }
                void await_resume() noexcept {}
            };

            final_awaiter final_suspend() noexcept { return {}; }
            void          unhandled_exception() { exc = stl::current_exception(); }
        };

    } // namespace detail

    template <typename T>
    class SpawnHandle {
    public:
        using value_type = T;

        struct promise_type
            : detail::spawn_promise_base
            , detail::spawn_value_storage<T> {
            SpawnHandle get_return_object() {
                return SpawnHandle(stl::coroutine_handle<promise_type>::from_promise(*this));
            }
        };

        using handle_type = stl::coroutine_handle<promise_type>;

        SpawnHandle() noexcept = default;
        explicit SpawnHandle(handle_type h) noexcept : m_handle(h) {}

        SpawnHandle(const SpawnHandle&)            = delete;
        SpawnHandle& operator=(const SpawnHandle&) = delete;

        SpawnHandle(SpawnHandle&& o) noexcept : m_handle(stl::exchange(o.m_handle, {})), m_executor(stl::exchange(o.m_executor, nullptr)) {}

        SpawnHandle& operator=(SpawnHandle&& o) noexcept {
            if (this != &o) {
                release();
                m_handle   = stl::exchange(o.m_handle, {});
                m_executor = stl::exchange(o.m_executor, nullptr);
            }
            return *this;
        }

        ~SpawnHandle() { release(); }

        bool await_ready() const noexcept { return !m_handle || m_handle.done(); }

        void await_suspend(stl::coroutine_handle<> awaiter) noexcept { m_handle.promise().continuation = awaiter; }

        T await_resume() {
            auto& p = m_handle.promise();
            if (p.exc)
                stl::rethrow_exception(p.exc);
            return p.take();
        }

        bool done() const noexcept { return !m_handle || m_handle.done(); }
        explicit operator bool() const noexcept { return static_cast<bool>(m_handle); }

        handle_type handle() const noexcept { return m_handle; }
        Executor*   executor() const noexcept { return m_executor; }
        void        set_executor(Executor* ex) noexcept { m_executor = ex; }

    private:
        // Pending runner can't be destroyed in-place: its IoAwaiter is registered with
        // the reactor via its address. Hand ownership off to the runner via handle_dropped
        // so it self-destroys at final_suspend.
        void release() noexcept {
            if (!m_handle)
                return;
            if (m_handle.done()) {
                m_handle.destroy();
            } else {
                m_handle.promise().handle_dropped = true;
            }
            m_handle   = {};
            m_executor = nullptr;
        }

        handle_type m_handle{};
        Executor*   m_executor = nullptr;
    };

    namespace detail {

        template <typename T>
        SpawnHandle<T> spawn_runner(Task<T> task) {
            if constexpr (std::is_void_v<T>) {
                co_await stl::move(task);
            } else {
                co_return co_await stl::move(task);
            }
        }

    } // namespace detail

    template <typename T>
    SpawnHandle<T> spawn(Executor& ex, Task<T> task) {
        auto handle = detail::spawn_runner<T>(stl::move(task));
        if (handle) {
            handle.set_executor(&ex);
            handle.handle().resume();
        }
        return handle;
    }

    template <typename T>
    T sync_wait(SpawnHandle<T>&& handle) {
        if (!handle.done())
            handle.executor()->run_until(handle.handle());
        auto& p = handle.handle().promise();
        if (p.exc)
            stl::rethrow_exception(p.exc);
        if constexpr (!std::is_void_v<T>) {
            return p.take();
        }
    }

} // namespace sap::async

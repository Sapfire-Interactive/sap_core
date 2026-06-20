#pragma once

#include "sap_core/stl/shared_ptr.h"
#include "sap_core/stl/utility.h"

#include <exception>

namespace sap::async {

    class StopSource;
    class StopToken;

    class CancelledError : public std::exception {
    public:
        const char* what() const noexcept override { return "operation cancelled"; }
    };

    namespace detail {

        struct stop_state;

        // Address is registered in stop_state's list while armed, so the node
        // is pinned: no copy, no move.
        struct stop_callback_node {
            stop_callback_node*           next  = nullptr;
            stop_callback_node*           prev  = nullptr;
            stop_state*                   state = nullptr;
            void                          (*fn)(void*) noexcept = nullptr;
            void*                         arg   = nullptr;

            stop_callback_node() noexcept                            = default;
            stop_callback_node(const stop_callback_node&)            = delete;
            stop_callback_node& operator=(const stop_callback_node&) = delete;
            stop_callback_node(stop_callback_node&&)                 = delete;
            stop_callback_node& operator=(stop_callback_node&&)      = delete;

            ~stop_callback_node() noexcept;
        };

        struct stop_state {
            bool                stopped = false;
            stop_callback_node* head    = nullptr;

            void link(stop_callback_node* cb) noexcept {
                cb->prev = nullptr;
                cb->next = head;
                if (head)
                    head->prev = cb;
                head      = cb;
                cb->state = this;
            }

            void unlink(stop_callback_node* cb) noexcept {
                if (cb->prev)
                    cb->prev->next = cb->next;
                else
                    head = cb->next;
                if (cb->next)
                    cb->next->prev = cb->prev;
                cb->state = nullptr;
                cb->prev = cb->next = nullptr;
            }

            bool request_stop() noexcept {
                if (stopped)
                    return false;
                stopped = true;
                while (head) {
                    auto* cb = head;
                    unlink(cb);
                    if (cb->fn)
                        cb->fn(cb->arg);
                }
                return true;
            }
        };

        inline stop_callback_node::~stop_callback_node() noexcept {
            if (state)
                state->unlink(this);
        }

    } // namespace detail

    class StopToken {
    public:
        StopToken() noexcept = default;

        bool stop_requested() const noexcept { return m_state && m_state->stopped; }
        bool stop_possible() const noexcept { return static_cast<bool>(m_state); }

        // Returns true iff stop was already requested (fn fired immediately, cb not linked).
        bool _arm(detail::stop_callback_node* cb, void (*fn)(void*) noexcept, void* arg) const noexcept {
            cb->fn  = fn;
            cb->arg = arg;
            if (!m_state)
                return false;
            if (m_state->stopped) {
                if (fn)
                    fn(arg);
                return true;
            }
            m_state->link(cb);
            return false;
        }

    private:
        friend class StopSource;
        explicit StopToken(stl::shared_ptr<detail::stop_state> s) noexcept : m_state(stl::move(s)) {}

        stl::shared_ptr<detail::stop_state> m_state;
    };

    class StopSource {
    public:
        StopSource() : m_state(stl::make_shared<detail::stop_state>()) {}

        StopSource(const StopSource&)            = delete;
        StopSource& operator=(const StopSource&) = delete;
        StopSource(StopSource&&) noexcept        = default;
        StopSource& operator=(StopSource&&) noexcept = default;
        ~StopSource()                            = default;

        StopToken token() const noexcept { return StopToken(m_state); }

        bool request_stop() noexcept { return m_state ? m_state->request_stop() : false; }
        bool stop_requested() const noexcept { return m_state ? m_state->stopped : false; }

    private:
        stl::shared_ptr<detail::stop_state> m_state;
    };

} // namespace sap::async

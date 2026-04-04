#pragma once

#include "sap_core/types.h"

#include <utility>

namespace stl {
    template <typename T, stl::size_t Capacity = 65536>
    class spsc_queue {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    public:
        bool try_push(const T& item) {
            const stl::size_t head = m_head.load(std::memory_order_relaxed);
            const stl::size_t next = (head + 1) & (Capacity - 1);
            if (next == m_tail.load(std::memory_order_acquire))
                return false;
            m_buffer[head] = item;
            m_head.store(next, std::memory_order_release);
            return true;
        }

        bool try_push(T&& item) {
            const stl::size_t head = m_head.load(std::memory_order_relaxed);
            const stl::size_t next = (head + 1) & (Capacity - 1);
            if (next == m_tail.load(std::memory_order_acquire))
                return false;
            m_buffer[head] = std::move(item);
            m_head.store(next, std::memory_order_release);
            return true;
        }

        bool try_pop(T& item) {
            const stl::size_t tail = m_tail.load(std::memory_order_relaxed);
            // empty?
            if (tail == m_head.load(std::memory_order_acquire))
                return false;
            item = m_buffer[tail];
            m_tail.store((tail + 1) & (Capacity - 1), std::memory_order_release);
            return true;
        }

    private:
        // Pad head and tail to separate cache lines - prevents false sharing
        // (would writing m_head would invalidate consumer cache like for m_tail)
        alignas(64) stl::atomic<stl::size_t> m_head{0};
        alignas(64) stl::atomic<stl::size_t> m_tail{0};
        stl::array<T, Capacity> m_buffer;
    };
} // namespace stl
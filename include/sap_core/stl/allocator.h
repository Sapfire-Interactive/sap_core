#pragma once

#include <cassert>
#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>
#include "stl/arena.h"

namespace stl {

    // STL-compatible linear/bump allocator adapter.
    // Wraps a linear_arena*. Deallocations are no-ops.
    template <class T>
    class linear_allocator {
    public:
        using value_type = T;
        using propagate_on_container_copy_assignment = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_swap = std::true_type;

        linear_allocator() noexcept : arena_(nullptr) {}
        explicit linear_allocator(linear_arena* a) noexcept : arena_(a) {}

        template <class U>
        linear_allocator(const linear_allocator<U>& other) noexcept : arena_(other.arena_) {}

        T* allocate(std::size_t n) {
            assert(arena_ && "linear_allocator requires a valid linear_arena");
            if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
                throw std::bad_alloc();
            void* p = arena_->allocate(n * sizeof(T), alignof(T));
            if (!p)
                throw std::bad_alloc();
            return static_cast<T*>(p);
        }

        void deallocate(T*, std::size_t) noexcept {}

        template <class U>
        struct rebind {
            using other = linear_allocator<U>;
        };

        template <class U>
        friend class linear_allocator;

        linear_arena* arena() const noexcept { return arena_; }

        bool operator==(const linear_allocator& rhs) const noexcept { return arena_ == rhs.arena_; }
        bool operator!=(const linear_allocator& rhs) const noexcept { return !(*this == rhs); }

    private:
        linear_arena* arena_;
    };

    // STL-compatible pool allocator adapter.
    // Uses a fixed_block_pool for single-element allocations (O(1) alloc + dealloc),
    // falls back to the underlying linear_arena for multi-element allocations.
    template <class T>
    class pool_allocator {
    public:
        using value_type = T;
        using propagate_on_container_copy_assignment = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_swap = std::true_type;

        pool_allocator() noexcept : arena_(nullptr), pool_(nullptr) {}
        explicit pool_allocator(linear_arena* a) : arena_(a), pool_(nullptr) {}

        template <class U>
        pool_allocator(const pool_allocator<U>& other) noexcept : arena_(other.arena_), pool_(nullptr) {}

        T* allocate(std::size_t n) {
            if (n == 1) {
                ensure_pool();
                return static_cast<T*>(pool_->allocate());
            }
            void* p = arena_->allocate(n * sizeof(T), alignof(T));
            if (!p)
                throw std::bad_alloc();
            return static_cast<T*>(p);
        }

        void deallocate(T* p, std::size_t n) noexcept {
            if (!p || n == 0)
                return;
            if (n == 1 && pool_) {
                pool_->deallocate(p);
            }
            // multi-element deallocations are no-ops on linear arenas
        }

        template <class U>
        struct rebind {
            using other = pool_allocator<U>;
        };

        template <class U>
        friend class pool_allocator;

        fixed_block_pool* pool() const noexcept { return pool_; }
        linear_arena* arena() const noexcept { return arena_; }

        bool operator==(const pool_allocator& rhs) const noexcept { return arena_ == rhs.arena_ && pool_ == rhs.pool_; }
        bool operator!=(const pool_allocator& rhs) const noexcept { return !(*this == rhs); }

    private:
        void ensure_pool() {
            if (pool_)
                return;
            void* mem = arena_->allocate(sizeof(fixed_block_pool), alignof(fixed_block_pool));
            if (!mem)
                throw std::bad_alloc();
            pool_ = new (mem) fixed_block_pool(arena_, sizeof(T));
        }

        linear_arena* arena_;
        fixed_block_pool* pool_;
    };

} // namespace stl

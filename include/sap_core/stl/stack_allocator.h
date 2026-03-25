#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include "stl/arena.h"

namespace stl {

    // Fixed-size arena with inline storage. No heap allocation.
    // Acts as a linear_arena but the backing buffer lives on the stack (or inline in the object).
    template <size_t N>
    class stack_arena {
    public:
        stack_arena() noexcept : cur_(buf_) {}

        stack_arena(const stack_arena&) = delete;
        stack_arena& operator=(const stack_arena&) = delete;
        stack_arena(stack_arena&&) = delete;
        stack_arena& operator=(stack_arena&&) = delete;

        void* allocate(size_t bytes, size_t align) {
            auto* aligned = reinterpret_cast<uint8_t*>(align_up(reinterpret_cast<size_t>(cur_), align));
            if (aligned + bytes > buf_ + N)
                return nullptr;
            cur_ = aligned + bytes;
            return aligned;
        }

        void deallocate(void*, size_t, size_t) noexcept {}

        void reset() noexcept { cur_ = buf_; }

        size_t capacity() const noexcept { return N; }
        size_t used() const noexcept { return static_cast<size_t>(cur_ - buf_); }
        size_t remaining() const noexcept { return N - used(); }

    private:
        alignas(alignof(std::max_align_t)) uint8_t buf_[N];
        uint8_t* cur_;
    };

    // STL-compatible allocator backed by a stack_arena.
    // The arena must outlive all containers using this allocator.
    template <class T>
    class stack_allocator {
    public:
        using value_type = T;
        // Do NOT propagate on copy — the destination must use its own arena.
        using propagate_on_container_copy_assignment = std::false_type;
        using propagate_on_container_move_assignment = std::false_type;
        using propagate_on_container_swap = std::false_type;

        stack_allocator() noexcept : arena_(nullptr) {}

        template <size_t N>
        explicit stack_allocator(stack_arena<N>& arena) noexcept :
            arena_(&arena), capacity_(N), allocate_fn_(&dispatch_allocate<N>), deallocate_fn_(&dispatch_deallocate<N>) {}

        template <class U>
        stack_allocator(const stack_allocator<U>& other) noexcept :
            arena_(other.arena_), capacity_(other.capacity_), allocate_fn_(other.allocate_fn_), deallocate_fn_(other.deallocate_fn_) {}

        T* allocate(std::size_t n) {
            assert(arena_ && "stack_allocator requires a valid stack_arena");
            void* p = allocate_fn_(arena_, n * sizeof(T), alignof(T));
            if (!p)
                throw std::bad_alloc();
            return static_cast<T*>(p);
        }

        void deallocate(T*, std::size_t) noexcept {}

        template <class U>
        struct rebind {
            using other = stack_allocator<U>;
        };

        template <class U>
        friend class stack_allocator;

        size_t capacity() const noexcept { return capacity_; }

        bool operator==(const stack_allocator& rhs) const noexcept { return arena_ == rhs.arena_; }
        bool operator!=(const stack_allocator& rhs) const noexcept { return !(*this == rhs); }

    private:
        using allocate_fn = void* (*)(void*, size_t, size_t);
        using deallocate_fn = void (*)(void*, void*, size_t, size_t);

        template <size_t N>
        static void* dispatch_allocate(void* arena, size_t bytes, size_t align) {
            return static_cast<stack_arena<N>*>(arena)->allocate(bytes, align);
        }

        template <size_t N>
        static void dispatch_deallocate(void* arena, void* p, size_t bytes, size_t align) {
            static_cast<stack_arena<N>*>(arena)->deallocate(p, bytes, align);
        }

        void* arena_;
        size_t capacity_{0};
        allocate_fn allocate_fn_{nullptr};
        deallocate_fn deallocate_fn_{nullptr};
    };

} // namespace stl

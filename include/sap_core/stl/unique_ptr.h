#pragma once

#include <cassert>
#include <memory>
#include "stl/arena.h"

namespace stl {

    // Deleter that calls the destructor but does not free memory.
    // Suitable for arena-allocated objects where deallocation is a bulk operation.
    template <typename T>
    struct arena_deleter {
        void operator()(T* ptr) const {
            if (ptr)
                ptr->~T();
        }
    };

    // unique_ptr with arena-aware deleter (destructor-only, no deallocation)
    template <typename T>
    using arena_unique_ptr = std::unique_ptr<T, arena_deleter<T>>;

    // Standard unique_ptr (default deleter)
    template <typename T>
    using unique_ptr = std::unique_ptr<T>;

    // Create a unique_ptr using the default allocator.
    template <typename T, typename... Args>
    unique_ptr<T> make_unique(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    // Create an arena-allocated unique_ptr.
    // The object is constructed via placement new in arena memory.
    // The deleter calls ~T() but does not free; memory is reclaimed on arena reset.
    template <typename T, typename... Args>
    arena_unique_ptr<T> make_unique(linear_arena& arena, Args&&... args) {
        void* mem = arena.allocate(sizeof(T), alignof(T));
        assert(mem && "make_unique: arena allocation failed");
        T* ptr = new (mem) T(std::forward<Args>(args)...);
        return arena_unique_ptr<T>(ptr);
    }

} // namespace stl

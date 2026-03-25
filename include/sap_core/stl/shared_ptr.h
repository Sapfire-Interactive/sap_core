#pragma once

#include <memory>

namespace stl {

    template <typename T>
    using shared_ptr = std::shared_ptr<T>;

    // Create a shared_ptr using a custom allocator for the control block + object.
    template <typename T, typename Allocator, typename... Args>
    shared_ptr<T> make_shared(const Allocator& alloc, Args&&... args) {
        return std::allocate_shared<T>(alloc, std::forward<Args>(args)...);
    }

    // Create a shared_ptr using the default allocator.
    template <typename T, typename... Args>
    shared_ptr<T> make_shared(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

} // namespace stl

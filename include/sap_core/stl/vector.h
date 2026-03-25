#pragma once

#include <cassert>
#include <memory>
#include <vector>

namespace stl {

    template <class T, class Allocator = std::allocator<T>>
    class vector : public std::vector<T, Allocator> {
    public:
        using base_type = std::vector<T, Allocator>;
        using allocator_type = Allocator;

        // Default constructor (requires default-constructible allocator)
        vector()
            requires std::is_default_constructible_v<Allocator>
            : base_type() {}

        // Construct with allocator
        explicit vector(const Allocator& alloc) : base_type(alloc) {}

        // Construct with count default-inserted elements
        vector(typename base_type::size_type count, const Allocator& alloc) : base_type(count, alloc) {}

        // Construct with count copies of value
        vector(typename base_type::size_type count, const T& value, const Allocator& alloc) : base_type(count, value, alloc) {}

        // Construct from iterator range
        template <class InputIt>
        vector(InputIt first, InputIt last, const Allocator& alloc) : base_type(first, last, alloc) {}

        // Construct from initializer list
        vector(std::initializer_list<T> init, const Allocator& alloc) : base_type(init, alloc) {}

        vector(const vector& other) : base_type(other) {}
        vector(vector&& other) noexcept : base_type(std::move(other)) {}

        vector& operator=(const vector& other) {
            base_type::operator=(other);
            return *this;
        }

        vector& operator=(vector&& other) noexcept {
            base_type::operator=(std::move(other));
            return *this;
        }

        // Inherit remaining constructors
        using base_type::base_type;
    };

    // Factory: create a vector with a given allocator
    template <class T, class Allocator>
    [[nodiscard]] inline vector<T, Allocator> make_vector(const Allocator& alloc) {
        return vector<T, Allocator>(alloc);
    }

} // namespace stl

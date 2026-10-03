#pragma once

#include <cassert>
#include <memory>
#include <vector>
#include <sap_core/stl/default_allocator.h>

// Optional project allocator policy. Without these definitions, the public
// API and allocator remain exactly the original std::allocator configuration.
#ifdef SAP_CORE_VECTOR_ALLOCATOR_HEADER
#include SAP_CORE_VECTOR_ALLOCATOR_HEADER
#endif
#ifndef SAP_CORE_VECTOR_DEFAULT_ALLOCATOR
#define SAP_CORE_VECTOR_DEFAULT_ALLOCATOR SAP_CORE_DEFAULT_ALLOCATOR
#endif

namespace stl {

    template <class T, class Allocator = SAP_CORE_VECTOR_DEFAULT_ALLOCATOR<T>>
    class vector : public std::vector<T, Allocator> {
    public:
        using base_type = std::vector<T, Allocator>;
        using allocator_type = Allocator;

        // Default constructor (requires default-constructible allocator)
        vector()
            requires std::is_default_constructible_v<Allocator>
            : base_type() {}

        vector()
            requires(!std::is_default_constructible_v<Allocator>)
        = delete;

        // Construct with allocator
        explicit vector(const Allocator& alloc) : base_type(alloc) {}

        // Project category tags select an allocator without becoming a count.
        template <class Tag>
            requires(std::is_enum_v<Tag> && std::is_constructible_v<Allocator, Tag>)
        explicit vector(Tag tag) : base_type(Allocator(tag)) {}

        // Construct with count default-inserted elements
        vector(typename base_type::size_type count, const Allocator& alloc = Allocator()) : base_type(count, alloc) {}

        // Construct with count copies of value
        vector(typename base_type::size_type count, const T& value, const Allocator& alloc = Allocator()) :
            base_type(count, value, alloc) {}

        // Construct from iterator range
        template <class InputIt>
        vector(InputIt first, InputIt last, const Allocator& alloc = Allocator()) : base_type(first, last, alloc) {}

        // Construct from initializer list
        vector(std::initializer_list<T> init, const Allocator& alloc) : base_type(init, alloc) {}
        vector(std::initializer_list<T> init) requires std::is_default_constructible_v<Allocator> : base_type(init) {}

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

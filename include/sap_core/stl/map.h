#pragma once

#include <cassert>
#include <functional>
#include <map>
#include <memory>

namespace stl {

    template <class Key, class T, class Compare = std::less<Key>, class Allocator = std::allocator<std::pair<const Key, T>>>
    class map : public std::map<Key, T, Compare, Allocator> {
    public:
        using base_type = std::map<Key, T, Compare, Allocator>;
        using allocator_type = Allocator;

        map()
            requires std::is_default_constructible_v<Allocator>
            : base_type() {}

        explicit map(const Allocator& alloc) : base_type(Compare{}, alloc) {}

        map(const Compare& comp, const Allocator& alloc) : base_type(comp, alloc) {}

        template <class InputIt>
        map(InputIt first, InputIt last, const Allocator& alloc) : base_type(first, last, Compare{}, alloc) {}

        map(std::initializer_list<std::pair<const Key, T>> init, const Allocator& alloc) : base_type(init, Compare{}, alloc) {}

        map(const map& other) : base_type(other) {}
        map(map&& other) noexcept : base_type(std::move(other)) {}

        map& operator=(const map& other) {
            base_type::operator=(other);
            return *this;
        }

        map& operator=(map&& other) noexcept {
            base_type::operator=(std::move(other));
            return *this;
        }

        using base_type::base_type;
    };

    template <class Key, class T, class Compare = std::less<Key>, class Allocator = std::allocator<std::pair<const Key, T>>>
    [[nodiscard]] inline map<Key, T, Compare, Allocator> make_map(const Allocator& alloc) {
        return map<Key, T, Compare, Allocator>(alloc);
    }

} // namespace stl

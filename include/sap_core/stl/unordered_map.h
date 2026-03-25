#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace stl {

    // Transparent string hash/equality for string-keyed maps
    struct string_hash {
        using is_transparent = void;
        size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
    };

    struct string_equal {
        using is_transparent = void;
        bool operator()(std::string_view a, std::string_view b) const { return a == b; }
    };

    template <class K, class V, class Hash = std::hash<K>, class KeyEqual = std::equal_to<K>,
              class Allocator = std::allocator<std::pair<const K, V>>>
    class unordered_map : public std::unordered_map<K, V, Hash, KeyEqual, Allocator> {
    public:
        using base_type = std::unordered_map<K, V, Hash, KeyEqual, Allocator>;
        using allocator_type = Allocator;

        unordered_map()
            requires std::is_default_constructible_v<Allocator>
            : base_type() {}

        explicit unordered_map(const Allocator& alloc) : base_type(0, Hash{}, KeyEqual{}, alloc) {}

        unordered_map(typename base_type::size_type bucket_count, const Allocator& alloc) :
            base_type(bucket_count, Hash{}, KeyEqual{}, alloc) {}

        unordered_map(typename base_type::size_type bucket_count, const Hash& hash, const Allocator& alloc) :
            base_type(bucket_count, hash, KeyEqual{}, alloc) {}

        unordered_map(const unordered_map& other) : base_type(other) {}
        unordered_map(unordered_map&& other) noexcept : base_type(std::move(other)) {}

        unordered_map& operator=(const unordered_map& other) {
            base_type::operator=(other);
            return *this;
        }

        unordered_map& operator=(unordered_map&& other) noexcept {
            base_type::operator=(std::move(other));
            return *this;
        }

        using base_type::base_type;
    };

    // Convenience alias for string-keyed unordered_maps with transparent lookup
    template <class V, class Allocator = std::allocator<std::pair<const std::string, V>>>
    using string_unordered_map = unordered_map<std::string, V, string_hash, string_equal, Allocator>;

    template <class K, class V, class Hash = std::hash<K>, class KeyEqual = std::equal_to<K>,
              class Allocator = std::allocator<std::pair<const K, V>>>
    [[nodiscard]] inline unordered_map<K, V, Hash, KeyEqual, Allocator> make_unordered_map(const Allocator& alloc) {
        return unordered_map<K, V, Hash, KeyEqual, Allocator>(alloc);
    }

} // namespace stl

#pragma once

#include <cassert>
#include <format>
#include <memory>
#include <string>
#include <string_view>

namespace stl {

    template <class Allocator = std::allocator<char>>
    class basic_string : public std::basic_string<char, std::char_traits<char>, Allocator> {
    public:
        using base_type = std::basic_string<char, std::char_traits<char>, Allocator>;
        using allocator_type = Allocator;

        // Default constructor (requires default-constructible allocator)
        basic_string()
            requires std::is_default_constructible_v<Allocator>
            : base_type() {}

        // Construct with allocator
        explicit basic_string(const Allocator& alloc) : base_type(alloc) {}

        // Construct from C string + allocator
        basic_string(const char* str, const Allocator& alloc) : base_type(str, alloc) {}

        // Construct from std::string + allocator
        basic_string(const std::string& str, const Allocator& alloc) : base_type(str.c_str(), alloc) {}

        // Construct from string_view + allocator
        basic_string(std::string_view sv, const Allocator& alloc) : base_type(sv.data(), sv.size(), alloc) {}

        // Implicit from C string (requires default-constructible allocator)
        basic_string(const char* str)
            requires std::is_default_constructible_v<Allocator>
            : base_type(str) {}

        // Implicit from string_view (requires default-constructible allocator)
        basic_string(std::string_view sv)
            requires std::is_default_constructible_v<Allocator>
            : base_type(sv.data(), sv.size()) {}

        basic_string(const basic_string& other) : base_type(other) {}
        basic_string(const base_type& other) : base_type(other) {}
        basic_string(basic_string&& other) noexcept : base_type(std::move(other)) {}
        basic_string(base_type&& other) noexcept : base_type(std::move(other)) {}

        basic_string& operator=(const basic_string& other) {
            base_type::operator=(other);
            return *this;
        }

        basic_string& operator=(basic_string&& other) noexcept {
            base_type::operator=(std::move(other));
            return *this;
        }

        basic_string& operator=(const char* str) {
            base_type::operator=(str);
            return *this;
        }

        basic_string& operator=(std::string_view sv) {
            base_type::assign(sv.data(), sv.size());
            return *this;
        }

        basic_string& operator=(const std::string& str) {
            base_type::operator=(str.c_str());
            return *this;
        }

        // Inherit remaining constructors
        using base_type::base_type;
    };

    // Factory: create a string with a given allocator
    template <class Allocator>
    [[nodiscard]] inline basic_string<Allocator> make_string(const Allocator& alloc) {
        return basic_string<Allocator>(alloc);
    }

    template <class Allocator>
    [[nodiscard]] inline basic_string<Allocator> make_string(const char* str, const Allocator& alloc) {
        return basic_string<Allocator>(str, alloc);
    }

    template <class Allocator>
    [[nodiscard]] inline basic_string<Allocator> make_string(std::string_view sv, const Allocator& alloc) {
        return basic_string<Allocator>(sv, alloc);
    }

    using string = basic_string<>;

} // namespace stl

template <class Allocator>
struct std::hash<stl::basic_string<Allocator>> {
    std::size_t operator()(const stl::basic_string<Allocator>& s) const noexcept {
        return std::hash<std::string_view>{}(std::string_view(s));
    }
};

template <class Allocator>
struct std::formatter<stl::basic_string<Allocator>> : std::formatter<std::string_view> {
    auto format(const stl::basic_string<Allocator>& s, std::format_context& ctx) const {
        return std::formatter<std::string_view>::format(std::string_view(s), ctx);
    }
};

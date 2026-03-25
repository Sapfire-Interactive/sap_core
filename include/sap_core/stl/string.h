#pragma once

#include <cassert>
#include <memory>
#include <string>
#include <string_view>

namespace stl {

    template <class Allocator = std::allocator<char>>
    class string : public std::basic_string<char, std::char_traits<char>, Allocator> {
    public:
        using base_type = std::basic_string<char, std::char_traits<char>, Allocator>;
        using allocator_type = Allocator;

        // Default constructor (requires default-constructible allocator)
        string()
            requires std::is_default_constructible_v<Allocator>
            : base_type() {}

        // Construct with allocator
        explicit string(const Allocator& alloc) : base_type(alloc) {}

        // Construct from C string + allocator
        string(const char* str, const Allocator& alloc) : base_type(str, alloc) {}

        // Construct from std::string + allocator
        string(const std::string& str, const Allocator& alloc) : base_type(str.c_str(), alloc) {}

        // Construct from string_view + allocator
        string(std::string_view sv, const Allocator& alloc) : base_type(sv.data(), sv.size(), alloc) {}

        // Implicit from C string (requires default-constructible allocator)
        string(const char* str)
            requires std::is_default_constructible_v<Allocator>
            : base_type(str) {}

        // Implicit from string_view (requires default-constructible allocator)
        string(std::string_view sv)
            requires std::is_default_constructible_v<Allocator>
            : base_type(sv.data(), sv.size()) {}

        string(const string& other) : base_type(other) {}
        string(const base_type& other) : base_type(other) {}
        string(string&& other) noexcept : base_type(std::move(other)) {}
        string(base_type&& other) noexcept : base_type(std::move(other)) {}

        string& operator=(const string& other) {
            base_type::operator=(other);
            return *this;
        }

        string& operator=(string&& other) noexcept {
            base_type::operator=(std::move(other));
            return *this;
        }

        string& operator=(const char* str) {
            base_type::operator=(str);
            return *this;
        }

        string& operator=(std::string_view sv) {
            base_type::assign(sv.data(), sv.size());
            return *this;
        }

        string& operator=(const std::string& str) {
            base_type::operator=(str.c_str());
            return *this;
        }

        // Inherit remaining constructors
        using base_type::base_type;
    };

    // Factory: create a string with a given allocator
    template <class Allocator>
    [[nodiscard]] inline string<Allocator> make_string(const Allocator& alloc) {
        return string<Allocator>(alloc);
    }

    template <class Allocator>
    [[nodiscard]] inline string<Allocator> make_string(const char* str, const Allocator& alloc) {
        return string<Allocator>(str, alloc);
    }

    template <class Allocator>
    [[nodiscard]] inline string<Allocator> make_string(std::string_view sv, const Allocator& alloc) {
        return string<Allocator>(sv, alloc);
    }

} // namespace stl

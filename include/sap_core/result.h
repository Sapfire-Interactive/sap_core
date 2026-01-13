#pragma once

#include <cassert>
#include <format>
#include <type_traits>
#include <utility>
#include "types.h"

namespace sap::stl {

    // Tag types for disambiguation
    struct success_tag_t {
        explicit success_tag_t() = default;
    };
    struct error_tag_t {
        explicit error_tag_t() = default;
    };

    inline constexpr success_tag_t success{};
    inline constexpr error_tag_t error{};

    // Result type for error handling
    template <typename T = u32, typename Et = std::string>
    class result {
    public:
        using value_type = T;
        using error_type = Et;

        // Default constructor - constructs with default value (if T is default constructible)
        result()
            requires std::is_default_constructible_v<T>
            : m_HasValue(true) {
            new (&m_Value) T();
        }

        // Success constructor
        template <typename... Args>
        result(success_tag_t, Args&&... args) : m_HasValue(true) {
            new (&m_Value) T(std::forward<Args>(args)...);
        }

        // Error constructor
        template <typename... Args>
        result(error_tag_t, Args&&... args) : m_HasValue(false) {
            new (&m_Error) Et(std::forward<Args>(args)...);
        }

        // Copy constructor
        result(const result& other)
            requires(std::is_copy_constructible_v<T> && std::is_copy_constructible_v<Et>)
            : m_HasValue(other.m_HasValue) {
            if (m_HasValue) {
                new (&m_Value) T(other.m_Value);
            } else {
                new (&m_Error) Et(other.m_Error);
            }
        }

        // Move constructor
        result(result&& other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<Et>)
            requires(std::is_move_constructible_v<T> && std::is_move_constructible_v<Et>)
            : m_HasValue(other.m_HasValue) {
            if (m_HasValue) {
                new (&m_Value) T(std::move(other.m_Value));
            } else {
                new (&m_Error) Et(std::move(other.m_Error));
            }
        }

        // Implicit conversion from value
        result(const T& value)
            requires std::is_copy_constructible_v<T>
            : m_HasValue(true) {
            new (&m_Value) T(value);
        }

        result(T&& value)
            requires std::is_move_constructible_v<T>
            : m_HasValue(true) {
            new (&m_Value) T(std::move(value));
        }

        // Destructor
        ~result() {
            if (m_HasValue) {
                m_Value.~T();
            } else {
                m_Error.~Et();
            }
        }

        // Copy assignment
        result& operator=(const result& other)
            requires(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T> && std::is_copy_constructible_v<Et> &&
                     std::is_copy_assignable_v<Et>)
        {
            if (this == &other)
                return *this;

            if (m_HasValue && other.m_HasValue) {
                m_Value = other.m_Value;
            } else if (!m_HasValue && !other.m_HasValue) {
                m_Error = other.m_Error;
            } else {
                // State change
                if (m_HasValue) {
                    m_Value.~T();
                } else {
                    m_Error.~Et();
                }
                m_HasValue = other.m_HasValue;
                if (m_HasValue) {
                    new (&m_Value) T(other.m_Value);
                } else {
                    new (&m_Error) Et(other.m_Error);
                }
            }
            return *this;
        }

        // Move assignment
        result& operator=(result&& other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T> &&
                                                   std::is_nothrow_move_constructible_v<Et> && std::is_nothrow_move_assignable_v<Et>)
            requires(std::is_move_constructible_v<T> && std::is_move_assignable_v<T> && std::is_move_constructible_v<Et> &&
                     std::is_move_assignable_v<Et>)
        {
            if (this == &other)
                return *this;

            if (m_HasValue && other.m_HasValue) {
                m_Value = std::move(other.m_Value);
            } else if (!m_HasValue && !other.m_HasValue) {
                m_Error = std::move(other.m_Error);
            } else {
                // State change
                if (m_HasValue) {
                    m_Value.~T();
                } else {
                    m_Error.~Et();
                }
                m_HasValue = other.m_HasValue;
                if (m_HasValue) {
                    new (&m_Value) T(std::move(other.m_Value));
                } else {
                    new (&m_Error) Et(std::move(other.m_Error));
                }
            }
            return *this;
        }

        // State checking
        [[nodiscard]] bool has_value() const noexcept { return m_HasValue; }
        [[nodiscard]] bool has_error() const noexcept { return !m_HasValue; }
        [[nodiscard]] explicit operator bool() const noexcept { return m_HasValue; }

        // Value access
        [[nodiscard]] T& value() & {
            assert(m_HasValue && "Attempting to access value of failed result");
            return m_Value;
        }

        [[nodiscard]] const T& value() const& {
            assert(m_HasValue && "Attempting to access value of failed result");
            return m_Value;
        }

        [[nodiscard]] T&& value() && {
            assert(m_HasValue && "Attempting to access value of failed result");
            return std::move(m_Value);
        }

        // Error access
        [[nodiscard]] Et& error() & {
            assert(!m_HasValue && "Attempting to access error of successful result");
            return m_Error;
        }

        [[nodiscard]] const Et& error() const& {
            assert(!m_HasValue && "Attempting to access error of successful result");
            return m_Error;
        }

        [[nodiscard]] Et&& error() && {
            assert(!m_HasValue && "Attempting to access error of successful result");
            return std::move(m_Error);
        }

        // Pointer-like access to value
        [[nodiscard]] T* operator->() {
            assert(m_HasValue && "Attempting to access value of failed result");
            return &m_Value;
        }

        [[nodiscard]] const T* operator->() const {
            assert(m_HasValue && "Attempting to access value of failed result");
            return &m_Value;
        }

        [[nodiscard]] T& operator*() & {
            assert(m_HasValue && "Attempting to access value of failed result");
            return m_Value;
        }

        [[nodiscard]] const T& operator*() const& {
            assert(m_HasValue && "Attempting to access value of failed result");
            return m_Value;
        }

        // Emplace functions
        template <typename... Args>
        void emplace_value(Args&&... args) {
            if (m_HasValue) {
                m_Value.~T();
            } else {
                m_Error.~Et();
            }
            m_HasValue = true;
            new (&m_Value) T(std::forward<Args>(args)...);
        }

        template <typename... Args>
        void emplace_error(Args&&... args) {
            if (m_HasValue) {
                m_Value.~T();
            } else {
                m_Error.~Et();
            }
            m_HasValue = false;
            new (&m_Error) Et(std::forward<Args>(args)...);
        }

    private:
        union {
            T m_Value;
            Et m_Error;
        };
        bool m_HasValue;
    };

    // Helper function to create error result with std::string message
    template <typename T = u32>
    [[nodiscard]] inline result<T, std::string> make_error(const char* msg) {
        return result<T, std::string>(error, msg);
    }

    // Helper function to create error result with formatted message
    template <typename T = u32, typename... Args>
    [[nodiscard]] inline result<T, std::string> make_error(std::format_string<Args...> fmt, Args&&... args) {
        auto formatted = std::format(fmt, std::forward<Args>(args)...);
        return result<T, std::string>(error, formatted);
    }

    // Helper function to create successful result
    template <typename... Args>
    [[nodiscard]] inline result<> result_success(Args&&... args) {
        return result<>(success, std::forward<Args>(args)...);
    }

} // namespace sf::stl
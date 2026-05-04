#pragma once

#include <cassert>
#include <format>
#include <string>
#include <type_traits>
#include <utility>

namespace stl {

    // Tag types for disambiguation
    struct success_tag_t {
        explicit success_tag_t() = default;
    };
    struct error_tag_t {
        explicit error_tag_t() = default;
    };

    inline constexpr success_tag_t success{};
    inline constexpr error_tag_t error{};

    // Result type for error handling.
    // T = value type, Et = error type (defaults to std::string).
    template <typename T = unsigned, typename Et = std::string>
    class result {
    public:
        using value_type = T;
        using error_type = Et;

        result()
            requires std::is_default_constructible_v<T>
            : m_has_value(true) {
            new (&m_value) T();
        }

        template <typename... Args>
        result(success_tag_t, Args&&... args) : m_has_value(true) {
            new (&m_value) T(std::forward<Args>(args)...);
        }

        template <typename... Args>
        result(error_tag_t, Args&&... args) : m_has_value(false) {
            new (&m_error) Et(std::forward<Args>(args)...);
        }

        result(const result& other)
            requires(std::is_copy_constructible_v<T> && std::is_copy_constructible_v<Et>)
            : m_has_value(other.m_has_value) {
            if (m_has_value)
                new (&m_value) T(other.m_value);
            else
                new (&m_error) Et(other.m_error);
        }

        result(result&& other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<Et>)
            requires(std::is_move_constructible_v<T> && std::is_move_constructible_v<Et>)
            : m_has_value(other.m_has_value) {
            if (m_has_value)
                new (&m_value) T(std::move(other.m_value));
            else
                new (&m_error) Et(std::move(other.m_error));
        }

        // Implicit conversion from value
        result(const T& value)
            requires std::is_copy_constructible_v<T>
            : m_has_value(true) {
            new (&m_value) T(value);
        }

        result(T&& value)
            requires std::is_move_constructible_v<T>
            : m_has_value(true) {
            new (&m_value) T(std::move(value));
        }

        ~result() {
            if (m_has_value)
                m_value.~T();
            else
                m_error.~Et();
        }

        result& operator=(const result& other)
            requires(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T> && std::is_copy_constructible_v<Et> &&
                     std::is_copy_assignable_v<Et>)
        {
            if (this == &other)
                return *this;
            if (m_has_value && other.m_has_value) {
                m_value = other.m_value;
            } else if (!m_has_value && !other.m_has_value) {
                m_error = other.m_error;
            } else {
                if (m_has_value)
                    m_value.~T();
                else
                    m_error.~Et();
                m_has_value = other.m_has_value;
                if (m_has_value)
                    new (&m_value) T(other.m_value);
                else
                    new (&m_error) Et(other.m_error);
            }
            return *this;
        }

        result& operator=(result&& other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T> &&
                                                   std::is_nothrow_move_constructible_v<Et> && std::is_nothrow_move_assignable_v<Et>)
            requires(std::is_move_constructible_v<T> && std::is_move_assignable_v<T> && std::is_move_constructible_v<Et> &&
                     std::is_move_assignable_v<Et>)
        {
            if (this == &other)
                return *this;
            if (m_has_value && other.m_has_value) {
                m_value = std::move(other.m_value);
            } else if (!m_has_value && !other.m_has_value) {
                m_error = std::move(other.m_error);
            } else {
                if (m_has_value)
                    m_value.~T();
                else
                    m_error.~Et();
                m_has_value = other.m_has_value;
                if (m_has_value)
                    new (&m_value) T(std::move(other.m_value));
                else
                    new (&m_error) Et(std::move(other.m_error));
            }
            return *this;
        }

        // State checking
        [[nodiscard]] bool has_value() const noexcept { return m_has_value; }
        [[nodiscard]] bool has_error() const noexcept { return !m_has_value; }
        [[nodiscard]] explicit operator bool() const noexcept { return m_has_value; }

        // Value access
        [[nodiscard]] T& value() & {
            assert(m_has_value && "Accessing value of failed result");
            return m_value;
        }
        [[nodiscard]] const T& value() const& {
            assert(m_has_value && "Accessing value of failed result");
            return m_value;
        }
        [[nodiscard]] T&& value() && {
            assert(m_has_value && "Accessing value of failed result");
            return std::move(m_value);
        }

        // Error access
        [[nodiscard]] Et& error() & {
            assert(!m_has_value && "Accessing error of successful result");
            return m_error;
        }
        [[nodiscard]] const Et& error() const& {
            assert(!m_has_value && "Accessing error of successful result");
            return m_error;
        }
        [[nodiscard]] Et&& error() && {
            assert(!m_has_value && "Accessing error of successful result");
            return std::move(m_error);
        }

        // Pointer-like access
        [[nodiscard]] T* operator->() {
            assert(m_has_value);
            return &m_value;
        }
        [[nodiscard]] const T* operator->() const {
            assert(m_has_value);
            return &m_value;
        }
        [[nodiscard]] T& operator*() & {
            assert(m_has_value);
            return m_value;
        }
        [[nodiscard]] const T& operator*() const& {
            assert(m_has_value);
            return m_value;
        }

        // Emplace
        template <typename... Args>
        void emplace_value(Args&&... args) {
            if (m_has_value)
                m_value.~T();
            else
                m_error.~Et();
            m_has_value = true;
            new (&m_value) T(std::forward<Args>(args)...);
        }

        template <typename... Args>
        void emplace_error(Args&&... args) {
            if (m_has_value)
                m_value.~T();
            else
                m_error.~Et();
            m_has_value = false;
            new (&m_error) Et(std::forward<Args>(args)...);
        }

    private:
        union {
            T m_value;
            Et m_error;
        };
        bool m_has_value;
    };

    // Helper: create error result from string
    template <typename T = unsigned>
    [[nodiscard]] inline result<T, std::string> make_error(const char* msg) {
        return result<T, std::string>(error, msg);
    }

    // Helper: create error result from format string
    template <typename T = unsigned, typename... Args>
    [[nodiscard]] inline result<T, std::string> make_error(std::format_string<Args...> fmt, Args&&... args) {
        return result<T, std::string>(error, std::format(fmt, std::forward<Args>(args)...));
    }

    // Helper: create successful result
    template <typename... Args>
    [[nodiscard]] inline result<> result_success(Args&&... args) {
        return result<>(success, std::forward<Args>(args)...);
    }

#define RESULT_CHECK(f)                                                                                                                    \
    auto res = f;                                                                                                                          \
    if (!res)                                                                                                                              \
        return res;

} // namespace stl

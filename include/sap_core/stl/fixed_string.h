#pragma once

#include <algorithm>
#include <cassert>
#include <compare>
#include <cstring>
#include <ostream>
#include <string>
#include <string_view>
#include "sap_core/stl/stack_allocator.h"

namespace stl {

    // A stack-allocated fixed-capacity string backed by a stack_arena + stack_allocator.
    // N is the maximum number of characters (excluding null terminator).
    // No heap allocation ever occurs; exceeding capacity is a fatal error.
    template <size_t N>
    class fixed_string {
    public:
        using value_type = char;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using reference = char&;
        using const_reference = const char&;
        using pointer = char*;
        using const_pointer = const char*;
        using iterator = char*;
        using const_iterator = const char*;

        fixed_string() noexcept : size_(0) {
            allocate_buf();
            buf_[0] = '\0';
        }

        fixed_string(const char* str) : size_(0) {
            allocate_buf();
            if (str) {
                size_ = std::strlen(str);
                assert(size_ <= N && "fixed_string: input exceeds capacity");
                std::memcpy(buf_, str, size_);
            }
            buf_[size_] = '\0';
        }

        fixed_string(const char* str, size_t len) : size_(len) {
            allocate_buf();
            assert(size_ <= N && "fixed_string: input exceeds capacity");
            std::memcpy(buf_, str, size_);
            buf_[size_] = '\0';
        }

        fixed_string(std::string_view sv) : size_(sv.size()) {
            allocate_buf();
            assert(size_ <= N && "fixed_string: input exceeds capacity");
            std::memcpy(buf_, sv.data(), size_);
            buf_[size_] = '\0';
        }

        fixed_string(size_t count, char ch) : size_(count) {
            allocate_buf();
            assert(size_ <= N && "fixed_string: input exceeds capacity");
            std::memset(buf_, ch, size_);
            buf_[size_] = '\0';
        }

        fixed_string(const fixed_string& other) : size_(other.size_) {
            allocate_buf();
            std::memcpy(buf_, other.buf_, size_ + 1);
        }

        template <size_t M>
        fixed_string(const fixed_string<M>& other) : size_(other.size()) {
            allocate_buf();
            assert(size_ <= N && "fixed_string: input exceeds capacity");
            std::memcpy(buf_, other.data(), size_ + 1);
        }

        fixed_string& operator=(const fixed_string& other) {
            if (this != &other) {
                size_ = other.size_;
                std::memcpy(buf_, other.buf_, size_ + 1);
            }
            return *this;
        }

        fixed_string& operator=(const char* str) {
            size_ = str ? std::strlen(str) : 0;
            assert(size_ <= N && "fixed_string: input exceeds capacity");
            if (str)
                std::memcpy(buf_, str, size_);
            buf_[size_] = '\0';
            return *this;
        }

        fixed_string& operator=(std::string_view sv) {
            size_ = sv.size();
            assert(size_ <= N && "fixed_string: input exceeds capacity");
            std::memcpy(buf_, sv.data(), size_);
            buf_[size_] = '\0';
            return *this;
        }

        // Capacity
        [[nodiscard]] size_t size() const noexcept { return size_; }
        [[nodiscard]] size_t length() const noexcept { return size_; }
        [[nodiscard]] static constexpr size_t capacity() noexcept { return N; }
        [[nodiscard]] static constexpr size_t max_size() noexcept { return N; }
        [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

        // Element access
        reference operator[](size_t i) { return buf_[i]; }
        const_reference operator[](size_t i) const { return buf_[i]; }

        reference at(size_t i) {
            assert(i < size_ && "fixed_string::at: index out of range");
            return buf_[i];
        }

        const_reference at(size_t i) const {
            assert(i < size_ && "fixed_string::at: index out of range");
            return buf_[i];
        }

        reference front() { return buf_[0]; }
        const_reference front() const { return buf_[0]; }
        reference back() { return buf_[size_ - 1]; }
        const_reference back() const { return buf_[size_ - 1]; }

        // Data access
        [[nodiscard]] const char* c_str() const noexcept { return buf_; }
        [[nodiscard]] const char* data() const noexcept { return buf_; }
        [[nodiscard]] char* data() noexcept { return buf_; }

        // Conversions
        [[nodiscard]] operator std::string_view() const noexcept { return {buf_, size_}; }
        [[nodiscard]] std::string_view view() const noexcept { return {buf_, size_}; }

        // Iterators
        iterator begin() noexcept { return buf_; }
        iterator end() noexcept { return buf_ + size_; }
        const_iterator begin() const noexcept { return buf_; }
        const_iterator end() const noexcept { return buf_ + size_; }
        const_iterator cbegin() const noexcept { return buf_; }
        const_iterator cend() const noexcept { return buf_ + size_; }

        // Modifiers
        void clear() noexcept {
            size_ = 0;
            buf_[0] = '\0';
        }

        void push_back(char ch) {
            assert(size_ < N && "fixed_string::push_back: capacity exceeded");
            buf_[size_++] = ch;
            buf_[size_] = '\0';
        }

        void pop_back() {
            assert(size_ > 0 && "fixed_string::pop_back: string is empty");
            buf_[--size_] = '\0';
        }

        fixed_string& append(std::string_view sv) {
            assert(size_ + sv.size() <= N && "fixed_string::append: capacity exceeded");
            std::memcpy(buf_ + size_, sv.data(), sv.size());
            size_ += sv.size();
            buf_[size_] = '\0';
            return *this;
        }

        fixed_string& append(const char* str) { return append(std::string_view(str)); }
        fixed_string& append(size_t count, char ch) {
            assert(size_ + count <= N && "fixed_string::append: capacity exceeded");
            std::memset(buf_ + size_, ch, count);
            size_ += count;
            buf_[size_] = '\0';
            return *this;
        }

        fixed_string& operator+=(std::string_view sv) { return append(sv); }
        fixed_string& operator+=(char ch) {
            push_back(ch);
            return *this;
        }

        void resize(size_t count, char ch = '\0') {
            assert(count <= N && "fixed_string::resize: capacity exceeded");
            if (count > size_)
                std::memset(buf_ + size_, ch, count - size_);
            size_ = count;
            buf_[size_] = '\0';
        }

        // Substr
        [[nodiscard]] std::string_view substr(size_t pos, size_t count = std::string_view::npos) const {
            assert(pos <= size_ && "fixed_string::substr: pos out of range");
            return std::string_view(buf_ + pos, std::min(count, size_ - pos));
        }

        // Find
        [[nodiscard]] size_t find(std::string_view sv, size_t pos = 0) const noexcept { return view().find(sv, pos); }

        [[nodiscard]] size_t find(char ch, size_t pos = 0) const noexcept { return view().find(ch, pos); }

        static constexpr size_t npos = std::string_view::npos;

        // Comparisons
        [[nodiscard]] bool operator==(const fixed_string& other) const noexcept { return view() == other.view(); }
        [[nodiscard]] auto operator<=>(const fixed_string& other) const noexcept { return view() <=> other.view(); }
        [[nodiscard]] bool operator==(std::string_view sv) const noexcept { return view() == sv; }
        [[nodiscard]] auto operator<=>(std::string_view sv) const noexcept { return view() <=> sv; }
        [[nodiscard]] bool operator==(const char* str) const noexcept { return view() == std::string_view(str); }

        // Stream output
        friend std::ostream& operator<<(std::ostream& os, const fixed_string& s) {
            return os.write(s.buf_, static_cast<std::streamsize>(s.size_));
        }

    private:
        void allocate_buf() {
            stack_allocator<char> alloc(arena_);
            buf_ = alloc.allocate(N + 1);
        }

        stack_arena<N + 1> arena_;
        char* buf_{nullptr};
        size_t size_{0};
    };

    // Concatenation
    template <size_t N, size_t M>
    [[nodiscard]] fixed_string<N + M> operator+(const fixed_string<N>& lhs, const fixed_string<M>& rhs) {
        fixed_string<N + M> result(lhs.view());
        result.append(rhs.view());
        return result;
    }

    template <size_t N>
    [[nodiscard]] fixed_string<N> operator+(const fixed_string<N>& lhs, std::string_view rhs) {
        fixed_string<N> result(lhs);
        result.append(rhs);
        return result;
    }

    template <size_t N>
    [[nodiscard]] fixed_string<N> operator+(std::string_view lhs, const fixed_string<N>& rhs) {
        fixed_string<N> result(lhs);
        result.append(rhs.view());
        return result;
    }

} // namespace stl

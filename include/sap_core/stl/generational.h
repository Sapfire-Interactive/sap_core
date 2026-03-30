#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>
#include "sap_core/types.h"

namespace stl {

    struct generational_index {
        u32 index = 0;
        u32 generation = 0;
    };

    // Allocates and recycles generational indices.
    // Free indices are reused with an incremented generation to detect stale references.
    template <class Allocator = std::allocator<u32>>
    class generational_index_allocator {
    public:
        generational_index_allocator()
            requires std::is_default_constructible_v<Allocator>
        = default;

        explicit generational_index_allocator(const Allocator& alloc) : m_entries(alloc), m_free_indices(alloc) {}

        generational_index allocate() {
            if (!m_free_indices.empty()) {
                u32 idx = m_free_indices.back();
                m_free_indices.pop_back();
                m_entries[idx].generation += 1;
                m_entries[idx].is_alive = true;
                return {idx, m_entries[idx].generation};
            }
            m_entries.push_back({true, 0});
            return {static_cast<u32>(m_entries.size()) - 1, 0};
        }

        void deallocate(generational_index index) {
            if (is_alive(index)) {
                m_entries[index.index].is_alive = false;
                m_free_indices.push_back(index.index);
            }
        }

        [[nodiscard]] bool is_alive(generational_index index) const {
            return index.index < m_entries.size() && m_entries[index.index].generation == index.generation &&
                m_entries[index.index].is_alive;
        }

    private:
        struct entry {
            bool is_alive = false;
            u32 generation = 0;
        };

        // Rebind allocator for internal types
        using entry_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<entry>;
        using u32_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<u32>;

        std::vector<entry, entry_alloc> m_entries;
        std::vector<u32, u32_alloc> m_free_indices;
    };

    // A sparse vector indexed by generational_index.
    // Entries are stored as optionals; stale indices return nullptr.
    template <typename T, class Allocator = std::allocator<T>>
    class generational_vector {
    public:
        struct entry {
            u32 generation;
            T value;
        };

        generational_vector()
            requires std::is_default_constructible_v<Allocator>
        = default;

        explicit generational_vector(const Allocator& alloc) : m_entries(alloc) {}

        void set(generational_index index, T val) {
            while (m_entries.size() <= index.index)
                m_entries.push_back(std::nullopt);
            u32 prev_gen = 0;
            if (auto& prev_entry = m_entries[index.index])
                prev_gen = prev_entry->generation;
            if (prev_gen > index.generation) {
                std::cerr << "Cannot set value at index " << index.index << ": previous generation is larger than current: " << prev_gen
                          << " > " << index.generation << std::endl;
                return;
            }
            m_entries[index.index] = std::optional<entry>{{
                .generation = index.generation,
                .value = std::move(val),
            }};
        }

        void remove(generational_index index) {
            if (index.index < m_entries.size())
                m_entries[index.index] = std::nullopt;
        }

        [[nodiscard]] T* get(generational_index index) {
            if (index.index >= m_entries.size())
                return nullptr;
            if (auto& e = m_entries[index.index]) {
                if (e->generation == index.generation)
                    return &e->value;
            }
            return nullptr;
        }

        [[nodiscard]] const T* get(generational_index index) const {
            if (index.index >= m_entries.size())
                return nullptr;
            if (auto& e = m_entries[index.index]) {
                if (e->generation == index.generation)
                    return &e->value;
            }
            return nullptr;
        }

        template <class IndexAllocator>
        [[nodiscard]] std::vector<generational_index>
        get_all_valid_indices(const generational_index_allocator<IndexAllocator>& allocator) const {
            std::vector<generational_index> result;
            for (u32 i = 0; i < m_entries.size(); ++i) {
                const auto& e = m_entries[i];
                if (!e)
                    continue;
                generational_index idx = {i, e->generation};
                if (allocator.is_alive(idx))
                    result.push_back(idx);
            }
            return result;
        }

        [[nodiscard]] size_t size() const { return m_entries.size(); }

        template <class IndexAllocator>
        [[nodiscard]] std::optional<std::tuple<generational_index, std::reference_wrapper<const T>>>
        get_first_valid_entry(const generational_index_allocator<IndexAllocator>& allocator) const {
            for (u32 i = 0; i < m_entries.size(); ++i) {
                const auto& e = m_entries[i];
                if (!e)
                    continue;
                generational_index idx = {i, e->generation};
                if (allocator.is_alive(idx))
                    return std::make_tuple(idx, std::cref(e->value));
            }
            return std::nullopt;
        }

        auto begin() { return m_entries.begin(); }
        auto end() { return m_entries.end(); }
        auto begin() const { return m_entries.begin(); }
        auto end() const { return m_entries.end(); }
        auto cbegin() const { return m_entries.cbegin(); }
        auto cend() const { return m_entries.cend(); }

    private:
        using opt_entry = std::optional<entry>;
        using entry_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<opt_entry>;

        std::vector<opt_entry, entry_alloc> m_entries;
    };

} // namespace stl

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>

namespace stl {

    inline constexpr size_t align_up(size_t x, size_t a) {
        assert((a & (a - 1)) == 0 && "Alignment must be a power of two");
        const size_t mask = a - 1;
        return (x + mask) & ~mask;
    }

    // Linear/bump arena allocator.
    // Allocations bump a cursor forward. Individual deallocations are no-ops;
    // memory is reclaimed only when the arena is reset().
    class linear_arena {
    public:
        linear_arena() : begin_(nullptr), cur_(nullptr), end_(nullptr), used_peak_(0), alloc_calls_(0) {}

        linear_arena(void* mem, size_t bytes) :
            begin_(static_cast<uint8_t*>(mem)), cur_(begin_), end_(begin_ + bytes), used_peak_(0), alloc_calls_(0) {}

        void reset_to(void* mem, size_t bytes) {
            begin_ = static_cast<uint8_t*>(mem);
            cur_ = begin_;
            end_ = begin_ + bytes;
            used_peak_ = 0;
            alloc_calls_ = 0;
        }

        void* allocate(size_t bytes, size_t align) {
            uint8_t* aligned = reinterpret_cast<uint8_t*>(align_up(reinterpret_cast<size_t>(cur_), align));
            if (aligned + bytes > end_)
                return nullptr;
            cur_ = aligned + bytes;
            size_t u = used();
            if (u > used_peak_)
                used_peak_ = u;
            ++alloc_calls_;
            return aligned;
        }

        void deallocate(void*, size_t, size_t) noexcept {}

        void reset() noexcept { cur_ = begin_; }

        size_t capacity() const noexcept { return static_cast<size_t>(end_ - begin_); }
        size_t used() const noexcept { return static_cast<size_t>(cur_ - begin_); }
        size_t peak() const noexcept { return used_peak_; }
        size_t alloc_calls() const noexcept { return alloc_calls_; }

    private:
        uint8_t* begin_;
        uint8_t* cur_;
        uint8_t* end_;
        size_t used_peak_;
        size_t alloc_calls_;
    };

    // Fixed-block pool backed by a linear_arena.
    // Maintains a free-list for O(1) allocate/deallocate of same-sized blocks.
    // Ideal for node-based containers (map, unordered_map, list).
    class fixed_block_pool {
    public:
        fixed_block_pool() = default;

        fixed_block_pool(linear_arena* arena, size_t block_size, size_t blocks_per_chunk = 256) :
            arena_(arena), blk_size_(block_size < sizeof(node) ? sizeof(node) : block_size), per_chunk_(blocks_per_chunk), free_(nullptr),
            chunks_allocated_(0) {}

        void* allocate() {
            if (!free_)
                add_chunk();
            node* n = free_;
            free_ = free_->next;
            return n;
        }

        void deallocate(void* p) noexcept {
            if (!p)
                return;
            auto* n = static_cast<node*>(p);
            n->next = free_;
            free_ = n;
        }

        size_t block_size() const noexcept { return blk_size_; }
        size_t chunks() const noexcept { return chunks_allocated_; }

    private:
        void add_chunk() {
            assert(arena_ && "fixed_block_pool requires a valid arena");
            void* mem = arena_->allocate(blk_size_ * per_chunk_, alignof(std::max_align_t));
            if (!mem)
                throw std::bad_alloc();
            auto* raw = static_cast<uint8_t*>(mem);
            for (size_t i = 0; i < per_chunk_; ++i) {
                auto* n = reinterpret_cast<node*>(raw + i * blk_size_);
                n->next = free_;
                free_ = n;
            }
            ++chunks_allocated_;
        }

        struct node {
            node* next;
        };

        linear_arena* arena_{nullptr};
        size_t blk_size_{0};
        size_t per_chunk_{0};
        node* free_{nullptr};
        size_t chunks_allocated_{0};
    };

} // namespace stl

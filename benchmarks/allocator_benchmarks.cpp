#include <benchmark/benchmark.h>

#include <sap_core/stl/allocator.h>
#include <sap_core/stl/arena.h>
#include <sap_core/stl/map.h>
#include <sap_core/stl/stack_allocator.h>
#include <sap_core/stl/vector.h>

#include <cstddef>
#include <cstdlib>
#include <map>
#include <memory>
#include <vector>

namespace {
    constexpr std::size_t kArenaBytes = 32 * 1024 * 1024;
    constexpr int kRangeMul = 8;
    constexpr int kRangeLo = 64;
    constexpr int kRangeHi = 65536;
}

static void BM_LinearArena_Alloc64(benchmark::State& state) {
    const std::size_t N = state.range(0);
    auto backing = std::make_unique<std::byte[]>(kArenaBytes);

    for (auto _ : state) {
        state.PauseTiming();
        stl::linear_arena arena{backing.get(), kArenaBytes};
        state.ResumeTiming();

        for (std::size_t i = 0; i < N; ++i) {
            void* p = arena.allocate(64, 8);
            benchmark::DoNotOptimize(p);
        }
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_LinearArena_Alloc64)->RangeMultiplier(kRangeMul)->Range(kRangeLo, kRangeHi);

static void BM_StackArena_Alloc64(benchmark::State& state) {
    const std::size_t N = state.range(0);
    auto arena = std::make_unique<stl::stack_arena<kArenaBytes>>();

    for (auto _ : state) {
        state.PauseTiming();
        arena->reset();
        state.ResumeTiming();

        for (std::size_t i = 0; i < N; ++i) {
            void* p = arena->allocate(64, 8);
            benchmark::DoNotOptimize(p);
        }
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_StackArena_Alloc64)->RangeMultiplier(kRangeMul)->Range(kRangeLo, kRangeHi);

static void BM_Malloc_AllocFree64(benchmark::State& state) {
    const std::size_t N = state.range(0);
    std::vector<void*> ptrs;
    ptrs.reserve(N);

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i)
            ptrs.push_back(std::malloc(64));
        for (void* p : ptrs)
            std::free(p);
        ptrs.clear();
    }
    state.SetItemsProcessed(state.iterations() * N * 2);
}
BENCHMARK(BM_Malloc_AllocFree64)->RangeMultiplier(kRangeMul)->Range(kRangeLo, kRangeHi);

static void BM_FixedBlockPool_AllocDealloc64(benchmark::State& state) {
    const std::size_t N = state.range(0);
    auto backing = std::make_unique<std::byte[]>(kArenaBytes);
    stl::linear_arena arena{backing.get(), kArenaBytes};
    stl::fixed_block_pool pool{&arena, 64};

    std::vector<void*> ptrs;
    ptrs.reserve(N);

    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i)
            ptrs.push_back(pool.allocate());
        for (void* p : ptrs)
            pool.deallocate(p);
        ptrs.clear();
    }
    state.SetItemsProcessed(state.iterations() * N * 2);
}
BENCHMARK(BM_FixedBlockPool_AllocDealloc64)->RangeMultiplier(kRangeMul)->Range(kRangeLo, kRangeHi);

static void BM_LinearArena_Reset(benchmark::State& state) {
    const std::size_t N = state.range(0);
    auto backing = std::make_unique<std::byte[]>(kArenaBytes);
    stl::linear_arena arena{backing.get(), kArenaBytes};

    for (auto _ : state) {
        state.PauseTiming();
        for (std::size_t i = 0; i < N; ++i)
            arena.allocate(64, 8);
        state.ResumeTiming();

        arena.reset();
        benchmark::DoNotOptimize(arena);
    }
}
BENCHMARK(BM_LinearArena_Reset)->RangeMultiplier(kRangeMul)->Range(kRangeLo, kRangeHi);

static void BM_Vector_StdAlloc_PushBack(benchmark::State& state) {
    const std::size_t N = state.range(0);
    for (auto _ : state) {
        std::vector<int> v;
        for (std::size_t i = 0; i < N; ++i)
            v.push_back(static_cast<int>(i));
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_Vector_StdAlloc_PushBack)->RangeMultiplier(kRangeMul)->Range(kRangeLo, kRangeHi);

static void BM_Vector_LinearAlloc_PushBack(benchmark::State& state) {
    const std::size_t N = state.range(0);
    auto backing = std::make_unique<std::byte[]>(kArenaBytes);

    for (auto _ : state) {
        state.PauseTiming();
        stl::linear_arena arena{backing.get(), kArenaBytes};
        state.ResumeTiming();

        stl::vector<int, stl::linear_allocator<int>> v{stl::linear_allocator<int>{&arena}};
        for (std::size_t i = 0; i < N; ++i)
            v.push_back(static_cast<int>(i));
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_Vector_LinearAlloc_PushBack)->RangeMultiplier(kRangeMul)->Range(kRangeLo, kRangeHi);

static void BM_Vector_StackAlloc_PushBack(benchmark::State& state) {
    const std::size_t N = state.range(0);
    auto arena = std::make_unique<stl::stack_arena<2 * 1024 * 1024>>();

    for (auto _ : state) {
        state.PauseTiming();
        arena->reset();
        state.ResumeTiming();

        stl::vector<int, stl::stack_allocator<int>> v{stl::stack_allocator<int>{*arena}};
        for (std::size_t i = 0; i < N; ++i)
            v.push_back(static_cast<int>(i));
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_Vector_StackAlloc_PushBack)->RangeMultiplier(kRangeMul)->Range(kRangeLo, kRangeHi);

static void BM_Map_StdAlloc_Insert(benchmark::State& state) {
    const std::size_t N = state.range(0);
    for (auto _ : state) {
        std::map<int, int> m;
        for (std::size_t i = 0; i < N; ++i)
            m.emplace(static_cast<int>(i), 0);
        benchmark::DoNotOptimize(m);
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_Map_StdAlloc_Insert)->RangeMultiplier(kRangeMul)->Range(kRangeLo, kRangeHi);

static void BM_Map_PoolAlloc_Insert(benchmark::State& state) {
    using pair_type = std::pair<const int, int>;
    const std::size_t N = state.range(0);
    auto backing = std::make_unique<std::byte[]>(kArenaBytes);

    for (auto _ : state) {
        state.PauseTiming();
        stl::linear_arena arena{backing.get(), kArenaBytes};
        state.ResumeTiming();

        stl::pool_allocator<pair_type> alloc{&arena};
        stl::map<int, int, std::less<int>, stl::pool_allocator<pair_type>> m{alloc};
        for (std::size_t i = 0; i < N; ++i)
            m.emplace(static_cast<int>(i), 0);
        benchmark::DoNotOptimize(m);
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_Map_PoolAlloc_Insert)->RangeMultiplier(kRangeMul)->Range(kRangeLo, kRangeHi);

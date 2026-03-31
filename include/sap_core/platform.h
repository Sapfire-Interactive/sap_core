#pragma once

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

// a single-cycle "do nothing useful" hint to the CPU.
// When spin-waiting (polling a queue in a tight loop), a raw empty loop
// causes two problems:
//   1. The CPU's speculative execution pipeline fills with useless
//      iterations that all get thrown away when data finally arrives.
//      This wastes power and actually SLOWS DOWN the producer core
//      on hyperthreaded CPUs (they share execution resources).
//   2. On x86, a tight spin-loop can starve the other hyperthread
//      on the same physical core.
// Typically costs 5-10 cycles - long enough to yield resources, short enough
// to avoid meaningful latency via syscalls.
inline void cpu_pause() {
#if defined(_MSC_VER) || defined(__x86_64__) || defined(__i386__)
    _mm_pause();
#elif defined(__aarch64__)
    // YIELD is ARM's equivalent — hints the core that this is a spin-wait.
    // ISB flushes the instruction pipeline, preventing speculative spam.
    asm volatile("yield" ::: "memory");
#else
    // Unknown architecture — compiler barrier only.
    // Prevents the compiler from optimizing away the spin loop,
    // but gives no hint to the hardware. Better than nothing.
    asm volatile("" ::: "memory");
#endif
}
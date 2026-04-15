#include "sap_core/guid.h"
#include <random>

namespace sap {
    u64 sap::generate_guid_u64() {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        static thread_local std::uniform_int_distribution<u64> dist;
        return dist(rng);
    }

    i64 sap::generate_guid_i64() {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        static thread_local std::uniform_int_distribution<i64> dist;
        return dist(rng);
    }
} // namespace sap

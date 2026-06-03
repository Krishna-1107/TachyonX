#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <iostream>

// Cross-Platform Intrinsic Headers
#if defined(_MSC_VER)
    #include <intrin.h>      // Windows MSVC
#else
    #include <x86intrin.h>   // GCC / Clang / MinGW
#endif

namespace MatchEngine {

class TSCClock {
public:
    // Force aggressive inlining so the measurement function call doesn't skew the results
    #if defined(_MSC_VER)
        __forceinline static uint64_t now() {
            return __rdtsc();
        }
    #else
        __attribute__((always_inline)) static inline uint64_t now() {
            return __rdtsc();
        }
    #endif
};

// Lock-free histogram to track latencies safely
class LatencyHistogram {
private:
    std::vector<uint64_t> latencies;

public:
    LatencyHistogram(size_t expected_size) {
        latencies.reserve(expected_size); // Pre-allocate to avoid runtime heap allocations
    }

    // Called on the hot path
    inline void record(uint64_t cycles) {
        latencies.push_back(cycles);
    }

    void printPercentiles() {
        if (latencies.empty()) return;

        std::sort(latencies.begin(), latencies.end());

        size_t count = latencies.size();
        uint64_t p50 = latencies[count * 0.50];
        uint64_t p90 = latencies[count * 0.90];
        uint64_t p99 = latencies[count * 0.99];
        uint64_t p999 = latencies[count * 0.999];
        uint64_t max = latencies.back();

        std::cout << "\n=========================================\n";
        std::cout << "      ENGINE LATENCY (CPU CLOCK CYCLES)  \n";
        std::cout << "=========================================\n";
        std::cout << "Samples:  " << count << "\n";
        std::cout << "50th %:   " << p50 << " cycles\n";
        std::cout << "90th %:   " << p90 << " cycles\n";
        std::cout << "99th %:   " << p99 << " cycles\n";
        std::cout << "99.9th %: " << p999 << " cycles\n";
        std::cout << "Worst:    " << max << " cycles\n";
        std::cout << "=========================================\n\n";
    }
};

} // namespace MatchEngine
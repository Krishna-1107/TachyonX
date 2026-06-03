#include "../include/Types.hpp"
#include "../include/MemoryPool.hpp" // Your Day 3 Implementation
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iomanip>

using namespace MatchEngine;

// Prevent the compiler from optimizing away our allocations
void preventOptimization(Order* ptr) {
    volatile Order* sink = ptr;
    (void)sink; 
}

// A lightweight statistical analyzer mimicking HdrHistogram
class LatencyAnalyzer {
private:
    std::vector<uint64_t> latencies;
    std::string name;

public:
    LatencyAnalyzer(std::string n, size_t reserveSize) : name(n) {
        latencies.reserve(reserveSize);
    }

    // Inline to ensure tracking overhead is absolutely minimal
    inline void record(uint64_t latency_ns) {
        latencies.push_back(latency_ns);
    }

    uint64_t getPercentile(double p) const {
        if (latencies.empty()) return 0;
        size_t index = static_cast<size_t>(latencies.size() * p);
        if (index >= latencies.size()) index = latencies.size() - 1;
        return latencies[index];
    }

    void analyzeAndPrint() {
        // Sort to easily extract exact percentiles
        std::sort(latencies.begin(), latencies.end());

        std::cout << "=================================================\n";
        std::cout << "=== " << name << " Latency Distribution ===\n";
        std::cout << "=================================================\n";
        
        std::vector<double> percentiles = {0.50, 0.90, 0.99, 0.999, 0.9999, 0.99999, 1.0};
        std::vector<std::string> labels = {"50.00%", "90.00%", "99.00%", "99.90%", "99.99%", "99.999%", "Max    "};

        for (size_t i = 0; i < percentiles.size(); ++i) {
            std::cout << "  " << labels[i] << " : " 
                      << std::setw(8) << getPercentile(percentiles[i]) << " ns\n";
        }
        
        printAsciiPlot(percentiles, labels);
    }

private:
    void printAsciiPlot(const std::vector<double>& percentiles, const std::vector<std::string>& labels) {
        std::cout << "\n  [ Tail Latency ASCII Plot ]\n";
        uint64_t max_val = latencies.back();
        if (max_val == 0) max_val = 1;

        for (size_t i = 0; i < percentiles.size(); ++i) {
            uint64_t val = getPercentile(percentiles[i]);
            // Scale bar to a max width of 40 characters
            int bar_length = static_cast<int>((static_cast<double>(val) / max_val) * 40.0);
            if (bar_length == 0 && val > 0) bar_length = 1; 

            std::cout << "  " << labels[i] << " |";
            for (int b = 0; b < bar_length; ++b) std::cout << "#";
            std::cout << " " << val << " ns\n";
        }
        std::cout << "\n";
    }
};

int main() {
    const size_t ITERATIONS = 1'000'000;
    
    // Warm up the CPU caches
    std::cout << "Warming up CPU and allocating memory...\n\n";
    LatencyAnalyzer heapAnalyzer("Standard OS Heap (new)", ITERATIONS);
    LatencyAnalyzer poolAnalyzer("O(1) Custom Memory Pool", ITERATIONS);
    
    std::vector<Order*> gc_tracker;
    gc_tracker.reserve(ITERATIONS);
    MemoryPool<Order> pool(ITERATIONS);

    // ---------------------------------------------------------
    // TEST 1: The OS Heap Allocator
    // ---------------------------------------------------------
    for (size_t i = 0; i < ITERATIONS; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        Order* order = new Order();
        preventOptimization(order);
        
        auto end = std::chrono::high_resolution_clock::now();
        
        uint64_t elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        heapAnalyzer.record(elapsed);
        gc_tracker.push_back(order);
    }

    // ---------------------------------------------------------
    // TEST 2: The Custom Free-List Memory Pool
    // ---------------------------------------------------------
    for (size_t i = 0; i < ITERATIONS; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        Order* order = pool.allocate();
        preventOptimization(order);
        
        auto end = std::chrono::high_resolution_clock::now();
        
        uint64_t elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        poolAnalyzer.record(elapsed);
    }

    // Analyze and print the statistical proofs
    heapAnalyzer.analyzeAndPrint();
    poolAnalyzer.analyzeAndPrint();

    // Cleanup to prevent memory leaks
    for (Order* ptr : gc_tracker) { delete ptr; }

    return 0;
}
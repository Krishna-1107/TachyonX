#pragma once

#include "Types.hpp"
#include "SPSCLockFreeRingBuffer.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

namespace MatchEngine {

class MarketDataParser {
public:
    // ==========================================
    // 1. GENERATE BINARY TEST DATA
    // ==========================================
    static void generateTestData(const std::string& filepath, size_t numOrders) {
        std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Could not open file for writing: " << filepath << "\n";
            return;
        }

        std::vector<RingBufferSlot> mockData;
        mockData.reserve(numOrders);

        // Generate alternating bids/asks to simulate a chaotic market
        for (size_t i = 1; i <= numOrders; ++i) {
            Price p = 100 + (i % 5); // Prices between 100 and 104
            Quantity q = (i % 10) * 10 + 10; // Qtys between 10 and 100
            Side s = (i % 2 == 0) ? Side::Buy : Side::Sell;
            
            mockData.emplace_back(i, p, q, s, OrderType::Limit);
        }

        // Add an aggressive market order at the end to trigger a massive sweep
        mockData.emplace_back(numOrders + 1, 0, 500, Side::Sell, OrderType::Market);

        // Write the entire vector to disk in one massive contiguous binary block
        file.write(reinterpret_cast<const char*>(mockData.data()), mockData.size() * sizeof(RingBufferSlot));
        file.close();
        
        std::cout << "[DATA] Successfully generated " << numOrders << " binary orders.\n";
    }

    // ==========================================
    // 2. ZERO-COPY PARSER & QUEUE INJECTOR
    // ==========================================
    template<size_t Capacity>
    static void streamToQueue(const std::string& filepath, SPSCLockFreeRingBuffer<RingBufferSlot, Capacity>& queue) {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Could not open binary file: " << filepath << "\n";
            return;
        }

        // Determine file size to allocate exact RAM needed
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // 1. Block Read: Pull the entire file into RAM at once (OS Independent)
        std::vector<char> buffer(fileSize);
        if (!file.read(buffer.data(), fileSize)) {
            std::cerr << "[ERROR] Failed to read binary block.\n";
            return;
        }

        // 2. Zero-Copy Cast: Treat the raw bytes as an array of structs
        const RingBufferSlot* orders = reinterpret_cast<const RingBufferSlot*>(buffer.data());
        size_t orderCount = fileSize / sizeof(RingBufferSlot);

        std::cout << "[PARSER] Ingested " << orderCount << " orders. Streaming to engine...\n";

        // 3. Blast into the lock-free queue
        for (size_t i = 0; i < orderCount; ++i) {
            // Spin-wait if the engine thread is slightly behind
            while (!queue.push(orders[i])) {} 
        }
    }
};

} // namespace MatchEngine
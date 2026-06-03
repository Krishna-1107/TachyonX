#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

// Linux-specific header for Thread Affinity (Hardware Pinning)
#if defined(__linux__)
#include <pthread.h>
#endif

// Your Engine Headers
#include "Types.hpp"
#include "MemoryPool.hpp"
#include "OrderBook.hpp"
#include "SPSCLockFreeRingBuffer.hpp" // Contains SPSCLockFreeRingBuffer
#include "MarketDataParser.hpp"
#include "Telemetry.hpp"

namespace MatchEngine {

// ==========================================
// THREAD PINNING HELPER
// ==========================================
void pinThreadToCore(std::thread& t, int core_id) {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    int rc = pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
#else
    std::cerr << "Thread pinning is only supported on Linux.\n";
#endif
}

} // namespace MatchEngine

using namespace MatchEngine;

int main() {
    std::cout << "[SYSTEM] Booting CoreCross Engine...\n";

    // ==========================================
    // 1. PRE-FLIGHT ALLOCATIONS (Zero OS interaction after this point)
    // ==========================================
    constexpr size_t POOL_CAPACITY = 10 * 1024 * 1024; // 10 Million Orders
    constexpr size_t MAX_PRICE_TICKS = 100000;         // e.g., prices from $0.00 to $1000.00
    constexpr size_t MAX_ORDER_ID = 20000000;          // Gateway internal ID limit

    std::cout << "[SYSTEM] Allocating Physical Memory Pool...\n";
    MemoryPool<Order> pool(POOL_CAPACITY);

    std::cout << "[SYSTEM] Initializing Order Dictionary & Price Arrays...\n";
    OrderBook book(MAX_PRICE_TICKS, MAX_ORDER_ID, pool.data());

    std::cout << "[SYSTEM] Establishing Cross-Core Ring Buffers...\n";
    // Must be powers of 2 for the bitwise modulo optimization
    SPSCLockFreeRingBuffer<RingBufferSlot, 4096> inboundQueue;
    SPSCLockFreeRingBuffer<Trade, 4096> outboundQueue;

    std::atomic<bool> systemRunning{true};

    // ==========================================
    // 2. THE ENGINE THREAD (Core 2)
    // ==========================================
    std::thread engineThread([&]() {
        std::vector<Trade> localFills;
        localFills.reserve(100); // Pre-allocate hot-path vector

        //benchmark
        LatencyHistogram histogram(50000);

        while (systemRunning.load(std::memory_order_relaxed)) {
            RingBufferSlot msg;
            
            // Poll the lock-free queue
            if (inboundQueue.pop(msg)) {

                // --- THE KILL SIGNAL TRAP ---
                // Tell the CPU that msg.id == 0 is highly unlikely (0).
                // This prevents the CPU pipeline from speculatively branching here.
                if (__builtin_expect(msg.id == 0, 0)) {
                    systemRunning.store(false, std::memory_order_release);
                    break; // Escape the polling loop
                }

                // 🔴 START TIMER (Read hardware clock)
                uint64_t start_cycles = TSCClock::now();
                
                // 1. Grab a blank hardware slot
                Index newIdx = pool.allocate();
                Order& incoming = pool[newIdx];
                
                // 2. Hydrate the struct from the raw network payload
                incoming.id       = msg.id;
                incoming.price    = msg.price;
                incoming.quantity = msg.quantity;
                incoming.side     = msg.side;
                incoming.type     = msg.type;
                incoming.next     = NULL_INDEX;
                incoming.prev     = NULL_INDEX;

                // 3. Fire the execution logic
                localFills.clear();
                book.processOrder(newIdx, pool, localFills);

                // 4. Publish any resulting fills to the outbound queue
                for (const auto& fill : localFills) {
                    // Spin-wait if outbound queue is temporarily full
                    while (!outboundQueue.push(fill)) {} 
                }

                // 🔴 STOP TIMER & RECORD
                uint64_t end_cycles = TSCClock::now();
                histogram.record(end_cycles - start_cycles);
            }
        }

        histogram.printPercentiles();
    });

    // Physically lock the Engine Thread to Core 2
    pinThreadToCore(engineThread, 2);

    // ==========================================
    // 3. THE NETWORK / GATEWAY THREAD (Core 1)
    // ==========================================
    // ==========================================
    // 3. THE NETWORK / GATEWAY THREAD (Core 1)
    // ==========================================
    std::thread networkThread([&]() {
        const std::string datafile = "market_traffic.dat";
        
        // 1. Generate 50,000 binary limit orders (Only runs once to create the file)
        MarketDataParser::generateTestData(datafile, 50000);

        // 2. Stream the binary file directly into the SPSC Ring Buffer
        MarketDataParser::streamToQueue(datafile, inboundQueue);

        // Send a kill signal to shut down the engine gracefully
        RingBufferSlot killSignal{0, 0, 0, Side::Buy, OrderType::Cancel};
        while(!inboundQueue.push(killSignal));
    });

    // Physically lock the Network Thread to Core 1
    pinThreadToCore(networkThread, 1);

    // THE MISSING CSV LOGIC IS BACK HERE
    std::ofstream csvFile("trades.csv", std::ios::trunc);
    if (csvFile.is_open()) {
        csvFile << "Timestamp_ms,BuyerID,SellerID,Price,Quantity\n";
    }

    const std::string GREEN = "\033[32m";
    const std::string RED = "\033[31m";
    const std::string CYAN = "\033[36m";
    const std::string RESET = "\033[0m";

    std::cout << "\n[LIVE FEED] Awaiting Executions...\n";

    auto startTime = std::chrono::steady_clock::now();

    // Loop until the Engine Thread hits the kill signal and flips systemRunning
    while (systemRunning.load(std::memory_order_relaxed)) { 
        Trade tradeOut;
        if (outboundQueue.pop(tradeOut)) {
            
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            
            if (csvFile.is_open()) {
                csvFile << ms << "," 
                        << tradeOut.buyerId << "," 
                        << tradeOut.sellerId << "," 
                        << tradeOut.matchPrice << "," 
                        << tradeOut.matchQuantity << "\n";
            }else{

                      std::cout << CYAN << "EXECTN [" << ms << "ms] " << RESET 
                      << "Bought by " << GREEN << tradeOut.buyerId << RESET 
                      << " | Sold by " << RED << tradeOut.sellerId << RESET
                      << " | Price: $" << tradeOut.matchPrice 
                      << " | Qty: " << tradeOut.matchQuantity << "\n";
            }
        }
    }

    if (csvFile.is_open()) {
        csvFile.flush();
        csvFile.close();
    }

    // Graceful Shutdown
    std::cout << "\n[SYSTEM] Initiating Shutdown...\n";
    
    networkThread.join();
    engineThread.join();

    // 5. Print the final state of the Order Book 
    book.printBook();

    std::cout << "[SYSTEM] Engine Terminated Safely.\n";
    return 0;
}
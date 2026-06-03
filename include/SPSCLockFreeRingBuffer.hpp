#pragma once

#include <atomic>
#include <cstddef>
#include <array>

namespace MatchEngine {

template <typename T, size_t Capacity>
class SPSCLockFreeRingBuffer {
    // The hardware requires the capacity to be a power of 2. 
    // This allows us to replace expensive modulo (%) with a 1-cycle bitwise AND (&).
    static_assert((Capacity != 0) && ((Capacity & (Capacity - 1)) == 0), 
                  "Capacity must be a power of 2");

private:
    // ==========================================
    // PRODUCER CACHE LINE (Network Thread / Core 1)
    // ==========================================
    // alignas(64) forces this block to start exactly on a new L1 cache line boundary.
    alignas(64) std::atomic<size_t> write_idx{0};
    size_t cached_read_idx{0}; 

    // ==========================================
    // CONSUMER CACHE LINE (Engine Thread / Core 2)
    // ==========================================
    // The second alignas(64) forces a massive physical gap, guaranteeing 
    // Core 1 and Core 2 never fight over the same cache line (No False Sharing).
    alignas(64) std::atomic<size_t> read_idx{0};
    size_t cached_write_idx{0}; 

    // ==========================================
    // PHYSICAL RING BUFFER
    // ==========================================
    alignas(64) std::array<T, Capacity> buffer;

public:
    SPSCLockFreeRingBuffer() = default;

    // Called ONLY by the Network Thread
    bool push(const T& item) {
        // Relaxed load: We own the write_idx, so no other thread can change it. No barrier needed.
        const size_t current_write = write_idx.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) & (Capacity - 1);

        // HFT Optimization: Check against our local, non-atomic cached read index first.
        if (next_write == cached_read_idx) {
            // We *think* the queue is full. Now we must incur the cost of going across 
            // the CPU interconnect bus to fetch the real index from Core 2.
            cached_read_idx = read_idx.load(std::memory_order_acquire);
            
            // If it is STILL full, we must back off and drop or spin.
            if (next_write == cached_read_idx) {
                return false; 
            }
        }

        // Write the raw packet data into the physical array
        buffer[current_write] = item;

        // Publish the update to Core 2.
        // memory_order_release acts as a hardware fence. It forces the CPU to 
        // drain its store buffer to the L3 cache, ensuring the data write happens 
        // BEFORE the index update becomes visible.
        write_idx.store(next_write, std::memory_order_release);
        return true;
    }

    // Called ONLY by the Engine Thread
    bool pop(T& item) {
        const size_t current_read = read_idx.load(std::memory_order_relaxed);

        if (current_read == cached_write_idx) {
            // We *think* the queue is empty. Fetch the real write index from Core 1.
            cached_write_idx = write_idx.load(std::memory_order_acquire);
            
            // If it is STILL empty, there is no new network traffic. Return to poll.
            if (current_read == cached_write_idx) {
                return false; 
            }
        }

        // Read the packet data
        item = buffer[current_read];

        // memory_order_release ensures our read from the array completes 
        // before we increment the read index, freeing up the slot.
        read_idx.store((current_read + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }
};

} // namespace MatchEngine
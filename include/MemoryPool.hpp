#pragma once

#include "Types.hpp"
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace MatchEngine {

template <typename T>
class MemoryPool {
private:
    std::vector<T> pool;
    std::vector<uint32_t> freeIndices;
    size_t capacity;

public:
    explicit MemoryPool(size_t poolCapacity) : capacity(poolCapacity) {
        pool.reserve(capacity);
        freeIndices.reserve(capacity); // Pre-allocated memory to avoid calling new during trading 
        
        for (size_t i = 0; i < capacity; ++i) {
            pool.emplace_back(); // constructs a default object inside the pool slot (handles demand paging overhead).
            freeIndices.push_back(static_cast<uint32_t>(capacity - 1 - i)); 
        }
    }

    // avoids accidental coping.
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    // O(1) Allocation
    Index allocate() {
        if (__builtin_expect(freeIndices.empty(), 0)) { // software level branch pridiction.
            throw std::bad_alloc(); 
        }
        
        Index index = freeIndices.back();
        freeIndices.pop_back();
        
        return index;
    }

    // O(1) Deallocation
    void deallocate(Index index) {
        freeIndices.push_back(index);
    }

    // 1. Allows the Engine Thread to hydrate the blank struct with raw network data
    T& operator[](Index index) {
        return pool[index];
    }

    // 2. Exposes the raw contiguous hardware array to the OrderBook for O(1) math
    T* data() {
        return pool.data();
    }
};

}
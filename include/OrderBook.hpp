#pragma once
#include "Types.hpp"
#include "PriceLevel.hpp"
#include "MemoryPool.hpp"
#include <vector>
#include <iostream>

namespace MatchEngine {

class OrderBook {
private:
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    
    // The WK Selph O(1) Lookup Array
    // Assumes monotonic Order IDs from the gateway (e.g., 1, 2, 3...)
    std::vector<Index> orderMap; 
    
    // Direct pointer to the Engine Thread's physical memory pool
    Order* poolBase; 

    Price bestBid{0};
    Price bestAsk{PRICE_MARKET};

public:
    OrderBook(size_t maxPriceTicks, size_t maxOrderId, Order* memoryPoolBase) 
        : poolBase(memoryPoolBase) {
        // Pre-allocate the geographical map at system startup
        bids.resize(maxPriceTicks);
        asks.resize(maxPriceTicks);
        orderMap.resize(maxOrderId, NULL_INDEX); 
    }

    void processOrder(Index incomingIdx, MemoryPool<Order>& pool, std::vector<Trade>& fills);

private:
    void matchBuy(Index incomingIdx, Order& incoming, MemoryPool<Order>& pool, std::vector<Trade>& fills);
    void matchSell(Index incomingIdx, Order& incoming, MemoryPool<Order>& pool, std::vector<Trade>& fills);

    // O(1) Registration
    void addOrder(Index orderIdx) {
        Order& order = poolBase[orderIdx];
        
        // 1. Register in the ID mapping array
        orderMap[order.id] = orderIdx;

        // 2. Append to the relevant Price Level and track best prices
        if (order.side == Side::Buy) {
            bids[order.price].append(orderIdx, poolBase);
            if (order.price > bestBid) {
                bestBid = order.price;
            }
        } else {
            asks[order.price].append(orderIdx, poolBase);
            if (order.price < bestAsk) {
                bestAsk = order.price;
            }
        }
    }

    // O(1) Targeted Cancellation
    Index cancelOrder(OrderId id) {
        Index orderIdx = orderMap[id];
        
        // Guard against double-cancels or invalid IDs
        if (__builtin_expect(orderIdx == NULL_INDEX, 0)) return NULL_INDEX; 

        Order& order = poolBase[orderIdx];

        // 1. Instruct the Price Level to unhook the node
        if (order.side == Side::Buy) {
            bids[order.price].remove(orderIdx, poolBase);
            
            // If the best price level is wiped out, fallback logic executes
            if (order.price == bestBid && bids[order.price].isEmpty()) {
                updateBestBid();
            }
        } else {
            asks[order.price].remove(orderIdx, poolBase);
            
            if (order.price == bestAsk && asks[order.price].isEmpty()) {
                updateBestAsk();
            }
        }

        // 2. Clear the lookup map
        orderMap[id] = NULL_INDEX;
        
        // Note: The actual return of `orderIdx` back to the freeIndices stack 
        // in the MemoryPool happens in the outer Engine Loop, not here.
        return orderIdx;
    }

    Price getBestBid() const { return bestBid; }
    Price getBestAsk() const { return bestAsk; }
    
    // Expose levels for the matching engine
    PriceLevel& getBidLevel(Price p) { return bids[p]; }
    PriceLevel& getAskLevel(Price p) { return asks[p]; }

private:
    void updateBestBid() {
        while (bestBid > 0 && bids[bestBid].isEmpty()) {
            bestBid--;
        }
    }

    void updateBestAsk() {
        while (bestAsk < asks.size() - 1 && asks[bestAsk].isEmpty()) {
            bestAsk++;
        }
    }

public:
    void printBook(size_t depth = 5) const {
        std::cout << "\n=========================================\n";
        std::cout << "          RESTING ORDER BOOK             \n";
        std::cout << "=========================================\n";
        
        std::cout << "\n--- ASKS (SELLERS) ---\n";
        size_t printedAsks = 0;
        // Traverse asks upwards from the lowest available seller
        for (Price p = bestAsk; p < asks.size() && printedAsks < depth; ++p) {
            if (!asks[p].isEmpty()) {
                Quantity totalVolume = 0;
                Index curr = asks[p].head;
                while (curr != NULL_INDEX) {
                    totalVolume += poolBase[curr].quantity;
                    curr = poolBase[curr].next; // Walk the linked list
                }
                std::cout << "[$" << p << "] \t Vol: " << totalVolume << "\n";
                printedAsks++;
            }
        }

        std::cout << "-----------------------------------------\n";

        std::cout << "--- BIDS (BUYERS) ---\n";
        size_t printedBids = 0;
        // Traverse bids downwards from the highest available buyer
        for (Price p = bestBid; p > 0 && printedBids < depth; --p) {
            if (!bids[p].isEmpty()) {
                Quantity totalVolume = 0;
                Index curr = bids[p].head;
                while (curr != NULL_INDEX) {
                    totalVolume += poolBase[curr].quantity;
                    curr = poolBase[curr].next; // Walk the linked list
                }
                std::cout << "[$" << p << "] \t Vol: " << totalVolume << "\n";
                printedBids++;
            }
        }
        std::cout << "=========================================\n\n";
    }
};

} 
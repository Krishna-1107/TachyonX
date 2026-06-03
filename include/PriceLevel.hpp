#pragma once
#include "Types.hpp"

namespace MatchEngine {

struct PriceLevel {
    Index head{NULL_INDEX};
    Index tail{NULL_INDEX};
    uint32_t volume{0};    
    uint32_t orderCount{0};

    // O(1) Time Priority Insertion
    void append(Index orderIdx, Order* pool) {
        Order& order = pool[orderIdx];
        order.next = NULL_INDEX;
        order.prev = tail;

        if (tail != NULL_INDEX) {
            pool[tail].next = orderIdx;
        } else {
            head = orderIdx;
        }
        tail = orderIdx;

        volume += order.quantity;
        orderCount++;
    }

    // O(1) Middle-of-list Cancellation
    void remove(Index orderIdx, Order* pool) {
        Order& order = pool[orderIdx];

        // Unhook from previous node
        if (order.prev != NULL_INDEX) {
            pool[order.prev].next = order.next;
        } else {
            head = order.next; 
        }

        // Unhook from next node
        if (order.next != NULL_INDEX) {
            pool[order.next].prev = order.prev;
        } else {
            tail = order.prev; 
        }

        // Scrub the node's pointers
        order.next = NULL_INDEX;
        order.prev = NULL_INDEX;

        volume -= order.quantity;
        orderCount--;
    }
    
    bool isEmpty() const { return head == NULL_INDEX; }
};

} 
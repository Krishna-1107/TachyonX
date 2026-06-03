#pragma once

#include <cstdint>
#include <limits>

namespace MatchEngine {

using Price = uint64_t;
using Quantity = uint32_t;
using OrderId = uint64_t;
using Index = uint32_t;

constexpr Price PRICE_MARKET = std::numeric_limits<Price>::max();
constexpr Index NULL_INDEX = std::numeric_limits<Index>::max(); 

enum class Side : uint8_t { Buy = 0, Sell = 1 };
enum class OrderType : uint8_t { Limit = 0, Market = 1, Cancel = 2 };

// owned only and only by engine thread so no need for cache line allignment.
struct alignas(32) Order {
    OrderId   id;        
    Price     price;     
    Quantity  quantity;  
    Index     next{NULL_INDEX}; // Intrusive relative pointer 
    Index     prev{NULL_INDEX}; // Intrusive relative pointer
    Side      side;      
    OrderType type;     
    
    Order() : id(0), price(0), quantity(0), side(Side::Buy), type(OrderType::Limit) {}
    
    Order(OrderId _id, Price _price, Quantity _qty, Side _side, OrderType _type)
        : id(_id), price(_price), quantity(_qty), side(_side), type(_type) {}
};

struct alignas(64) RingBufferSlot {
    OrderId   id;        
    Price     price;     
    Quantity  quantity;  
    Side      side;      
    OrderType type;      
    
    RingBufferSlot() = default;
    
    RingBufferSlot(OrderId _id, Price _price, Quantity _qty, Side _side, OrderType _type)
        : id(_id), price(_price), quantity(_qty), side(_side), type(_type) {}
};

struct Trade {
    OrderId  buyerId;
    OrderId  sellerId;
    Price    matchPrice;
    Quantity matchQuantity;
};

}
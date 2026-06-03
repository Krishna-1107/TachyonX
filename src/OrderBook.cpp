#include "OrderBook.hpp"
#include <algorithm>

namespace MatchEngine {

void OrderBook::processOrder(Index incomingIdx, MemoryPool<Order>& pool, std::vector<Trade>& fills) {
    Order& incoming = poolBase[incomingIdx];

    // Route 1: Explicit Cancellation
    if (incoming.type == OrderType::Cancel) {
        Index targetIdx = cancelOrder(incoming.id); 
        if (targetIdx != NULL_INDEX) {
            pool.deallocate(targetIdx); // Recycle the resting order
        }
        pool.deallocate(incomingIdx);   // Recycle the cancel message
        return;
    }

    // Route 2: Crossover Matching
    if (incoming.side == Side::Buy) {
        matchBuy(incomingIdx, incoming, pool, fills);
    } else {
        matchSell(incomingIdx, incoming, pool, fills);
    }

    // Route 3: Resting or Dropping Unfilled Quantity
    if (incoming.quantity > 0) {
        if (incoming.type == OrderType::Limit) {
            addOrder(incomingIdx); 
        } else if (incoming.type == OrderType::Market) {
            pool.deallocate(incomingIdx);
        }
    } else {
        // The incoming order was fully filled during matching; recycle it immediately
        pool.deallocate(incomingIdx);
    }
}

void OrderBook::matchBuy(Index incomingIdx, Order& incoming, MemoryPool<Order>& pool, std::vector<Trade>& fills) {
    while (incoming.quantity > 0 && bestAsk != PRICE_MARKET) {
        
        if (incoming.type == OrderType::Limit && incoming.price < bestAsk) {
            break; 
        }

        PriceLevel& askLevel = asks[bestAsk];
        Index currIdx = askLevel.head;

        while (currIdx != NULL_INDEX && incoming.quantity > 0) {
            Order& restingAsk = poolBase[currIdx];
            
            // Cache the next index before potential memory destruction
            Index nextIdx = restingAsk.next; 

            Quantity tradedQty = std::min(incoming.quantity, restingAsk.quantity);

            fills.push_back({
                incoming.id,      
                restingAsk.id,    
                bestAsk,          
                tradedQty         
            });

            incoming.quantity -= tradedQty;
            restingAsk.quantity -= tradedQty;

            if (restingAsk.quantity == 0) {
                askLevel.remove(currIdx, poolBase);
                orderMap[restingAsk.id] = NULL_INDEX;
                pool.deallocate(currIdx); 
            }

            currIdx = nextIdx;
        }

        if (askLevel.isEmpty()) {
            updateBestAsk();
        }
    }
}

void OrderBook::matchSell(Index incomingIdx, Order& incoming, MemoryPool<Order>& pool, std::vector<Trade>& fills) {
    while (incoming.quantity > 0 && bestBid != 0) {
        
        if (incoming.type == OrderType::Limit && incoming.price > bestBid) {
            break; 
        }

        PriceLevel& bidLevel = bids[bestBid];
        Index currIdx = bidLevel.head;

        while (currIdx != NULL_INDEX && incoming.quantity > 0) {
            Order& restingBid = poolBase[currIdx];
            
            // Cache the next index before potential memory destruction
            Index nextIdx = restingBid.next; 

            Quantity tradedQty = std::min(incoming.quantity, restingBid.quantity);

            fills.push_back({
                restingBid.id,    // Buyer is the resting order
                incoming.id,      // Seller is the incoming order
                bestBid,          
                tradedQty         
            });

            incoming.quantity -= tradedQty;
            restingBid.quantity -= tradedQty;

            if (restingBid.quantity == 0) {
                bidLevel.remove(currIdx, poolBase);
                orderMap[restingBid.id] = NULL_INDEX;
                pool.deallocate(currIdx); 
            }

            currIdx = nextIdx;
        }

        if (bidLevel.isEmpty()) {
            updateBestBid();
        }
    }
}

} // namespace MatchEngine
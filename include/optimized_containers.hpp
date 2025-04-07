#pragma once

#include <tbb/concurrent_unordered_map.h>
#include <boost/lockfree/queue.hpp>

namespace goquant {

// Lock-free order book implementation
template<typename Price, typename Size>
class LockFreeOrderBook {
public:
    void addOrder(Price price, Size size) {
        orders_.insert({price, size});
    }
    
    void removeOrder(Price price) {
        orders_.erase(price);
    }
    
private:
    tbb::concurrent_unordered_map<Price, Size> orders_;
};

// Lock-free message queue for market data
template<typename T>
class MarketDataQueue {
public:
    MarketDataQueue() : queue_(1000) {}  // Buffer size of 1000
    
    void push(const T& data) {
        queue_.push(data);
    }
    
    bool try_pop(T& result) {
        return queue_.pop(result);
    }
    
private:
    boost::lockfree::queue<T> queue_;
};

} // namespace goquant 
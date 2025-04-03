#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>
#include "websocket_server.hpp"

namespace goquant
{

    struct OrderBookLevel
    {
        double price;
        double amount;
    };

    struct OrderBook
    {
        std::string instrument_name;
        std::vector<OrderBookLevel> bids;
        std::vector<OrderBookLevel> asks;
        std::string timestamp;
    };

    class MarketData
    {
    public:
        using OrderBookCallback = std::function<void(const OrderBook &)>;
        using TickerCallback = std::function<void(const std::string &, double)>;

        MarketData(std::shared_ptr<WebSocketServer> ws_server);
        ~MarketData();

        // Market Data Subscription
        bool subscribeOrderBook(const std::string &instrument_name);
        bool subscribeTicker(const std::string &instrument_name);

        // Market Data Access
        OrderBook getOrderBook(const std::string &instrument_name) const;
        double getLastPrice(const std::string &instrument_name) const;

        // Callback Registration
        void setOrderBookCallback(OrderBookCallback callback);
        void setTickerCallback(TickerCallback callback);

    private:
        std::shared_ptr<WebSocketServer> ws_server_;
        std::map<std::string, OrderBook> orderbooks_;
        std::map<std::string, double> last_prices_;
        mutable std::mutex data_mutex_;

        OrderBookCallback orderbook_callback_;
        TickerCallback ticker_callback_;

        void handleOrderBookUpdate(const std::string &message);
        void handleTickerUpdate(const std::string &message);
        void processWebSocketMessage(const std::string &message);
    };

} // namespace goquant
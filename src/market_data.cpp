#include "market_data.hpp"
#include <iostream>

namespace goquant
{

    MarketData::MarketData(std::shared_ptr<WebSocketServer> ws_server)
        : ws_server_(ws_server)
    {
    }

    MarketData::~MarketData()
    {
    }

    bool MarketData::subscribeOrderBook(const std::string &instrument_name)
    {
        return ws_server_->subscribe("book", instrument_name);
    }

    bool MarketData::subscribeTicker(const std::string &instrument_name)
    {
        return ws_server_->subscribe("ticker", instrument_name);
    }

    OrderBook MarketData::getOrderBook(const std::string &instrument_name) const
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = orderbooks_.find(instrument_name);
        if (it != orderbooks_.end())
        {
            return it->second;
        }
        return OrderBook{};
    }

    double MarketData::getLastPrice(const std::string &instrument_name) const
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        auto it = last_prices_.find(instrument_name);
        if (it != last_prices_.end())
        {
            return it->second;
        }
        return 0.0;
    }

    void MarketData::setOrderBookCallback(OrderBookCallback callback)
    {
        orderbook_callback_ = callback;
    }

    void MarketData::setTickerCallback(TickerCallback callback)
    {
        ticker_callback_ = callback;
    }

    void MarketData::handleOrderBookUpdate(const std::string &message)
    {
        try
        {
            auto json = nlohmann::json::parse(message);
            if (!json.contains("params") || !json["params"].contains("data"))
            {
                return;
            }

            auto &data = json["params"]["data"];
            OrderBook book;
            book.instrument_name = data["instrument_name"];
            book.timestamp = data["timestamp"];

            for (const auto &bid : data["bids"])
            {
                OrderBookLevel level;
                level.price = bid[0];
                level.amount = bid[1];
                book.bids.push_back(level);
            }

            for (const auto &ask : data["asks"])
            {
                OrderBookLevel level;
                level.price = ask[0];
                level.amount = ask[1];
                book.asks.push_back(level);
            }

            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                orderbooks_[book.instrument_name] = book;
            }

            if (orderbook_callback_)
            {
                orderbook_callback_(book);
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error processing orderbook update: " << e.what() << std::endl;
        }
    }

    void MarketData::handleTickerUpdate(const std::string &message)
    {
        try
        {
            auto json = nlohmann::json::parse(message);
            if (!json.contains("params") || !json["params"].contains("data"))
            {
                return;
            }

            auto &data = json["params"]["data"];
            std::string instrument_name = data["instrument_name"];
            double last_price = data["last_price"];

            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                last_prices_[instrument_name] = last_price;
            }

            if (ticker_callback_)
            {
                ticker_callback_(instrument_name, last_price);
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error processing ticker update: " << e.what() << std::endl;
        }
    }

    void MarketData::processWebSocketMessage(const std::string &message)
    {
        try
        {
            auto json = nlohmann::json::parse(message);
            if (!json.contains("method"))
            {
                return;
            }

            std::string method = json["method"];
            if (method == "subscription")
            {
                if (json["params"]["channel"].get<std::string>().find("book") != std::string::npos)
                {
                    handleOrderBookUpdate(message);
                }
                else if (json["params"]["channel"].get<std::string>().find("ticker") != std::string::npos)
                {
                    handleTickerUpdate(message);
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error processing WebSocket message: " << e.what() << std::endl;
        }
    }

} // namespace goquant
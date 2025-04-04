#include "order_manager.hpp"
#include <spdlog/spdlog.h>
#include <mutex>

namespace goquant
{

    OrderManager::OrderManager(std::shared_ptr<DeribitClient> client) : client_(client) {}

    bool OrderManager::placeOrder(const std::string &instrument_name,
                                  const std::string &side,
                                  double amount,
                                  const std::string &type,
                                  double price)
    {
        try
        {
            nlohmann::json response = client_->placeOrder(instrument_name, side, amount, type, price);
            if (!response.empty() && response.contains("result") &&
                response["result"].contains("order") &&
                response["result"]["order"].contains("order_id"))
            {
                last_order_id_ = response["result"]["order"]["order_id"].get<std::string>();

                if (type == "limit")
                {
                    static std::mutex mutex_;
                    std::lock_guard<std::mutex> lock(mutex_);
                    active_orders_[last_order_id_] = response["result"]["order"];
                }
                return true;
            }
            return false;
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to place order: {}", e.what());
            return false;
        }
    }

    bool OrderManager::modifyOrder(const std::string &order_id,
                                   double new_amount,
                                   double new_price)
    {
        try
        {
            static std::mutex mutex_;
            std::lock_guard<std::mutex> lock(mutex_);

            // Check if order exists
            auto it = active_orders_.find(order_id);
            if (it == active_orders_.end())
            {
                return false;
            }

            const std::string &instrument_name = it->second["instrument_name"].get<std::string>();
            nlohmann::json response = client_->modifyOrder(order_id, instrument_name, new_price, new_amount);

            // Update the active orders map with the new order details
            if (response.contains("result") && response["result"].contains("order"))
            {
                active_orders_[order_id] = response["result"]["order"];
                return true;
            }
            return false;
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to modify order: {}", e.what());
            return false;
        }
    }

    bool OrderManager::cancelOrder(const std::string &order_id)
    {
        try
        {
            nlohmann::json response = client_->cancelOrder(order_id);
            if (!response.empty())
            {
                static std::mutex mutex_;
                std::lock_guard<std::mutex> lock(mutex_);
                active_orders_.erase(order_id);
                return true;
            }
            return false;
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to cancel order: {}", e.what());
            return false;
        }
    }

    const nlohmann::json &OrderManager::getOrder(const std::string &order_id) const
    {
        static const nlohmann::json empty_json;
        static std::mutex mutex_;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = active_orders_.find(order_id);
        if (it != active_orders_.end())
        {
            return it->second;
        }
        return empty_json;
    }

    const std::map<std::string, nlohmann::json> &OrderManager::getActiveOrders() const
    {
        return active_orders_;
    }

} // namespace goquant
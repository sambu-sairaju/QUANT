#pragma once

#include <string>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include "deribit_client.hpp"

namespace goquant
{
    class OrderManager
    {
    public:
        explicit OrderManager(std::shared_ptr<DeribitClient> client);

        bool placeOrder(const std::string &instrument_name,
                        const std::string &side,
                        double amount,
                        const std::string &type,
                        double price);

        bool modifyOrder(const std::string &order_id,
                         double new_amount,
                         double new_price);

        bool cancelOrder(const std::string &order_id);

        const nlohmann::json &getOrder(const std::string &order_id) const;
        const std::map<std::string, nlohmann::json> &getActiveOrders() const;
        std::string getLastOrderId() const { return last_order_id_; }

    private:
        std::shared_ptr<DeribitClient> client_;
        std::map<std::string, nlohmann::json> active_orders_;
        std::string last_order_id_;
    };

} // namespace goquant
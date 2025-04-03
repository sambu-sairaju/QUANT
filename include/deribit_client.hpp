#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

namespace goquant
{
    class DeribitClient
    {
    public:
        DeribitClient();
        ~DeribitClient();

        void authenticate();
        void refreshToken();
        bool isAuthenticated() const { return is_authenticated_; }

        nlohmann::json getOrderbook(const std::string &instrument_name, int depth);
        nlohmann::json placeOrder(const std::string &instrument_name,
                                  const std::string &side,
                                  double amount,
                                  const std::string &type,
                                  double price);
        nlohmann::json modifyOrder(const std::string &order_id,
                                   const std::string &instrument_name,
                                   double new_price,
                                   double new_amount);
        nlohmann::json cancelOrder(const std::string &order_id);
        nlohmann::json getPositions();
        nlohmann::json getInstrument(const std::string &instrument_name);

    private:
        static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);
        nlohmann::json makeRequest(const std::string &method,
                                   const std::string &path,
                                   const nlohmann::json &params);
        void ensureAuthenticated();

        CURL *curl_;
        bool is_authenticated_;
        std::string access_token_;
        std::string refresh_token_;
        long token_expiry_time_;
    };
}
#include "deribit_client.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <performance_monitor.hpp>
#include <spdlog/spdlog.h>

namespace goquant
{
    DeribitClient::DeribitClient()
        : curl_(nullptr), is_authenticated_(false), token_expiry_time_(0)
    {
        curl_ = curl_easy_init();
        if (!curl_)
        {
            throw std::runtime_error("Failed to initialize CURL");
        }
    }

    DeribitClient::~DeribitClient()
    {
        if (curl_)
        {
            curl_easy_cleanup(curl_);
        }
    }

    void DeribitClient::authenticate()
    {
        const char *client_id = std::getenv("DERIBIT_CLIENT_ID");
        const char *client_secret = std::getenv("DERIBIT_CLIENT_SECRET");

        if (!client_id || !client_secret)
        {
            throw std::runtime_error("DERIBIT_CLIENT_ID and DERIBIT_CLIENT_SECRET environment variables must be set");
        }

        nlohmann::json params = {
            {"client_id", client_id},
            {"client_secret", client_secret},
            {"grant_type", "client_credentials"},
            {"scope", "session:testnet"}};

        auto response = makeRequest("public/auth", "", params);
        if (response.contains("error"))
        {
            throw std::runtime_error("Authentication failed: " + response["error"]["message"].get<std::string>());
        }

        access_token_ = response["result"]["access_token"].get<std::string>();
        refresh_token_ = response["result"]["refresh_token"].get<std::string>();
        token_expiry_time_ = std::time(nullptr) + response["result"]["expires_in"].get<long>();
        is_authenticated_ = true;
    }

    void DeribitClient::refreshToken()
    {
        if (refresh_token_.empty())
        {
            throw std::runtime_error("No refresh token available");
        }

        nlohmann::json params = {
            {"grant_type", "refresh_token"},
            {"refresh_token", refresh_token_}};

        auto response = makeRequest("public/auth", "", params);
        if (response.contains("error"))
        {
            throw std::runtime_error("Token refresh failed: " + response["error"]["message"].get<std::string>());
        }

        access_token_ = response["result"]["access_token"].get<std::string>();
        refresh_token_ = response["result"]["refresh_token"].get<std::string>();
        token_expiry_time_ = std::time(nullptr) + response["result"]["expires_in"].get<long>();
        is_authenticated_ = true;
    }

    void DeribitClient::ensureAuthenticated()
    {
        if (!is_authenticated_ || std::time(nullptr) >= token_expiry_time_ - 60) // Refresh if less than 1 minute remaining
        {
            if (refresh_token_.empty())
            {
                authenticate();
            }
            else
            {
                refreshToken();
            }
        }
    }

    nlohmann::json DeribitClient::getInstrument(const std::string &instrument_name)
    {
        nlohmann::json params = {
            {"instrument_name", instrument_name}};

        return makeRequest("public/get_instrument", "", params);
    }

    nlohmann::json DeribitClient::getOrderbook(const std::string &instrument_name, int depth)
    {
        nlohmann::json params = {
            {"instrument_name", instrument_name},
            {"depth", depth}};

        return makeRequest("public/get_order_book", "", params);
    }

    nlohmann::json DeribitClient::placeOrder(const std::string &instrument_name,
                                             const std::string &side,
                                             double amount,
                                             const std::string &type,
                                             double price)
    {
        auto& monitor = PerformanceMonitor::getInstance();
        monitor.startOperation("order_placement");
        
        ensureAuthenticated();

        // For BTC-PERPETUAL, amount must be a multiple of 10
        if (instrument_name == "BTC-PERPETUAL")
        {
            // Round to nearest multiple of 10
            amount = std::round(amount / 10.0) * 10.0;
            if (amount < 10.0)
            {
                amount = 10.0; // Minimum amount is 10
            }
        }

        nlohmann::json params = {
            {"instrument_name", instrument_name},
            {"amount", amount},
            {"type", type},
            {"label", "goquant_order"},
            {"reduce_only", false}};

        // Add limit order specific parameters
        if (type == "limit")
        {
            // Round price to 2 decimal places
            price = std::round(price * 100.0) / 100.0;
            params["price"] = price;
            params["post_only"] = true;
            params["time_in_force"] = "good_til_cancelled";
        }

        std::string method = side == "buy" ? "private/buy" : "private/sell";
        nlohmann::json result = makeRequest(method, "", params);
        
        monitor.endOperation("order_placement");
        return result;
    }

    nlohmann::json DeribitClient::modifyOrder(const std::string &order_id,
                                              const std::string &instrument_name,
                                              double new_price,
                                              double new_amount)
    {
        ensureAuthenticated();

        nlohmann::json params = {
            {"order_id", order_id},
            {"instrument_name", instrument_name},
            {"amount", new_amount},
            {"price", new_price}};

        return makeRequest("private/edit", "", params);
    }

    nlohmann::json DeribitClient::cancelOrder(const std::string &order_id)
    {
        ensureAuthenticated();

        nlohmann::json params = {
            {"order_id", order_id}};

        return makeRequest("private/cancel", "", params);
    }

    nlohmann::json DeribitClient::getPositions()
    {
        ensureAuthenticated();
        return makeRequest("private/get_positions", "", nlohmann::json::object());
    }

    size_t DeribitClient::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        ((std::string *)userp)->append((char *)contents, size * nmemb);
        return size * nmemb;
    }

    nlohmann::json DeribitClient::makeRequest(const std::string &method,
                                              const std::string &path,
                                              const nlohmann::json &params)
    {
        std::string url = "https://test.deribit.com/api/v2/" + method;
        std::string response_string;

        // Create JSON-RPC request
        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", method},
            {"params", params}};

        std::string request_data = request.dump();

        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl_, CURLOPT_POST, 1L);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        if (is_authenticated_ && !access_token_.empty())
        {
            std::string auth_header = "Authorization: Bearer " + access_token_;
            headers = curl_slist_append(headers, auth_header.c_str());
        }

        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, request_data.c_str());

        // Remove or comment out these debug prints
        // std::cout << "Sending request: " << request.dump() << std::endl;
        // std::cout << "Received response: " << response << std::endl;

        CURLcode res = curl_easy_perform(curl_);
        curl_slist_free_all(headers);

        if (res != CURLE_OK)
        {
            throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
        }

        try
        {
            auto response = nlohmann::json::parse(response_string);
            if (response.contains("error") && !response["error"].is_null())
            {
                throw std::runtime_error("API error: " + response["error"]["message"].get<std::string>());
            }
            return response;
        }
        catch (const nlohmann::json::parse_error &e)
        {
            throw std::runtime_error("Failed to parse response: " + std::string(e.what()));
        }
    }

} // namespace goquant
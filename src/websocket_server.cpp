#include "websocket_server.hpp"
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <iostream>
#include <sstream>
#include <performance_monitor.hpp>
#include <spdlog/spdlog.h>

namespace goquant
{
    namespace beast = boost::beast;
    namespace websocket = beast::websocket;
    namespace net = boost::asio;
    namespace ssl = boost::asio::ssl;
    namespace http = beast::http;

    WebSocketServer::WebSocketServer()
        : ioc_(),
          ssl_ctx_(ssl::context::tlsv12_client),
          ws_(ioc_, ssl_ctx_),
          resolver_(ioc_),
          subscriptions_(),
          message_callback_(nullptr),
          connection_callback_(nullptr)
    {
        // Initialize SSL context
        ssl_ctx_.set_verify_mode(ssl::verify_none);
        ssl_ctx_.set_default_verify_paths();
    }

    bool WebSocketServer::connect(const std::string &host, const std::string &port)
    {
        try
        {
            std::cout << "Resolving host: " << host << ":" << port << std::endl;
            auto const results = resolver_.resolve(host, port);
            if (results.empty())
            {
                std::cerr << "Failed to resolve host" << std::endl;
                return false;
            }

            std::cout << "Connecting to WebSocket server..." << std::endl;
            auto ep = beast::get_lowest_layer(ws_).connect(results);
            std::cout << "Connected to: " << ep.address().to_string() << ":" << ep.port() << std::endl;

            // Perform SSL handshake
            std::cout << "Performing SSL handshake..." << std::endl;
            if (!SSL_set1_host(ws_.next_layer().native_handle(), host.c_str()))
            {
                std::cerr << "Failed to set SNI hostname" << std::endl;
                return false;
            }
            ws_.next_layer().handshake(ssl::stream_base::client);

            // Perform WebSocket handshake
            std::cout << "Performing WebSocket handshake..." << std::endl;
            ws_.handshake(host, "/ws/api/v2");
            std::cout << "WebSocket handshake successful" << std::endl;

            // Set User-Agent for the WebSocket handshake
            ws_.set_option(websocket::stream_base::decorator(
                [](websocket::request_type &req)
                {
                    req.set(http::field::user_agent, "GoQuant/1.0");
                }));

            // Start reading messages
            doRead();

            if (connection_callback_)
            {
                connection_callback_(true);
            }

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error connecting to WebSocket server: " << e.what() << std::endl;
            if (connection_callback_)
            {
                connection_callback_(false);
            }
            return false;
        }
    }

    void WebSocketServer::disconnect()
    {
        try
        {
            std::cout << "Disconnecting from WebSocket server..." << std::endl;
            if (ws_.is_open())
            {
                ws_.close(websocket::close_code::normal);
                std::cout << "WebSocket connection closed" << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error closing websocket: " << e.what() << std::endl;
        }

        if (connection_callback_)
        {
            connection_callback_(false);
        }
    }

    bool WebSocketServer::subscribe(const std::string &channel, const std::string &instrument)
    {
        try
        {
            std::cout << "Subscribing to " << channel << " for " << instrument << std::endl;
            nlohmann::json request = {
                {"jsonrpc", "2.0"},
                {"method", "public/subscribe"},
                {"id", 42},
                {"params", {{"channels", {channel + "." + instrument}}}}};

            std::string message = request.dump();
            std::cout << "Sending subscription request: " << message << std::endl;
            ws_.write(net::buffer(message));

            subscriptions_.insert(channel + "." + instrument);
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error subscribing to channel: " << e.what() << std::endl;
            return false;
        }
    }

    void WebSocketServer::unsubscribe(const std::string &channel, const std::string &instrument)
    {
        try
        {
            std::cout << "Unsubscribing from " << channel << " for " << instrument << std::endl;
            nlohmann::json request = {
                {"jsonrpc", "2.0"},
                {"method", "public/unsubscribe"},
                {"id", 42},
                {"params", {{"channels", {channel + "." + instrument}}}}};

            std::string message = request.dump();
            std::cout << "Sending unsubscribe request: " << message << std::endl;
            ws_.write(net::buffer(message));

            subscriptions_.erase(channel + "." + instrument);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error unsubscribing from channel: " << e.what() << std::endl;
        }
    }

    void WebSocketServer::doRead()
    {
        ws_.async_read(
            buffer_,
            [this](beast::error_code ec, std::size_t)
            {
                if (ec)
                {
                    std::cerr << "read: " << ec.message() << std::endl;
                    if (connection_callback_)
                    {
                        connection_callback_(false);
                    }
                    return;
                }

                try
                {
                    std::string message = beast::buffers_to_string(buffer_.data());
                    buffer_.consume(buffer_.size());

                    onMessage(message);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error processing message: " << e.what() << std::endl;
                }

                doRead();
            });
    }

    void WebSocketServer::onMessage(const std::string& message)
    {
        auto& monitor = PerformanceMonitor::getInstance();
        monitor.startOperation("websocket_message");

        try {
            // Process message
            if (message_callback_) {
                message_callback_(message);
            }

            monitor.endOperation("websocket_message");
        }
        catch (const std::exception& e) {
            monitor.endOperation("websocket_message");
            spdlog::error("Error processing WebSocket message: {}", e.what());
        }
    }

    void WebSocketServer::setMessageCallback(MessageCallback callback)
    {
        message_callback_ = callback;
    }

    void WebSocketServer::setConnectionCallback(ConnectionCallback callback)
    {
        connection_callback_ = callback;
    }

} // namespace goquant
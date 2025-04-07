#pragma once

#include <string>
#include <functional>
#include <thread>
#include <set>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <nlohmann/json.hpp>

namespace goquant
{
    class WebSocketServer
    {
    public:
        using MessageCallback = std::function<void(const std::string &)>;
        using ConnectionCallback = std::function<void(bool)>;

        WebSocketServer();
        ~WebSocketServer() = default;

        bool connect(const std::string &host, const std::string &port);
        void disconnect();
        bool subscribe(const std::string &channel, const std::string &instrument);
        void unsubscribe(const std::string &channel, const std::string &instrument);
        void setMessageCallback(MessageCallback callback);
        void setConnectionCallback(ConnectionCallback callback);
        void onMessage(const std::string& message);

    private:
        void doRead();

        boost::asio::io_context ioc_;
        boost::asio::ssl::context ssl_ctx_;
        boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> ws_;
        boost::asio::ip::tcp::resolver resolver_;
        boost::beast::flat_buffer buffer_;
        std::set<std::string> subscriptions_;
        MessageCallback message_callback_;
        ConnectionCallback connection_callback_;
    };

} // namespace goquant
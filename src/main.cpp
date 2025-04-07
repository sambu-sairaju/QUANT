#include "deribit_client.hpp"
#include "websocket_server.hpp"
#include "order_manager.hpp"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cmath>
#include <string>
#include <limits>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "performance_monitor.hpp"
#include "performance_analyzer.hpp"
#include "thread_pool.hpp"
#include <spdlog/spdlog.h>

void clearInputBuffer()
{
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void displayMainMenu()
{
    std::cout << "\n=== Deribit Trading System ===" << std::endl;
    std::cout << "1. Place Order" << std::endl;
    std::cout << "2. Cancel Order" << std::endl;
    std::cout << "3. Modify Order" << std::endl;
    std::cout << "4. Get Orderbook" << std::endl;
    std::cout << "5. View Current Positions" << std::endl;
    std::cout << "6. Real-time Market Data" << std::endl;
    std::cout << "7. View Active Orders" << std::endl;
    std::cout << "8. Run Performance Test" << std::endl;
    std::cout << "0. Exit" << std::endl;
    std::cout << "Enter your choice: ";
}

void displayActiveOrders(std::shared_ptr<goquant::OrderManager> order_manager)
{
    std::cout << "\n=== Active Limit Orders ===" << std::endl;
    std::cout << "╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║ Order ID          Side     Amount        Price         ║" << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════╣" << std::endl;

    const auto &active_orders = order_manager->getActiveOrders();
    if (active_orders.empty())
    {
        std::cout << "║              No active limit orders found              ║" << std::endl;
    }
    else
    {
        for (const auto &[order_id, order] : active_orders)
        {
            std::cout << "║ " << std::left << std::setw(16) << order_id
                      << std::setw(8) << order["direction"].get<std::string>()
                      << std::right << std::setw(9) << std::fixed << std::setprecision(2)
                      << order["amount"].get<double>()
                      << "    $" << std::setw(9) << order["price"].get<double>()
                      << " ║" << std::endl;
        }
    }
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;
}

void placeOrder(std::shared_ptr<goquant::DeribitClient> client, std::shared_ptr<goquant::OrderManager> order_manager)
{
    std::string instrument, side, type;
    double price = 0.0, amount;

    std::cout << "\n=== Place Order ===" << std::endl;

    std::cout << "Enter instrument (e.g., BTC-PERPETUAL): ";
    std::getline(std::cin, instrument);

    // Get instrument details to show contract size
    try
    {
        auto instrument_info = client->getInstrument(instrument);
        double contract_size = instrument_info["result"]["contract_size"].get<double>();
        std::cout << "Minimum contract size for " << instrument << ": " << contract_size << std::endl;
        std::cout << "Amount must be a multiple of " << contract_size << std::endl;

        // Round the contract size to avoid floating point issues
        contract_size = std::round(contract_size * 100.0) / 100.0;

        // Validate amount is multiple of contract size
        std::cout << "Enter amount (must be multiple of " << contract_size << "): ";
        std::cin >> amount;
        clearInputBuffer();

        // Round amount to 2 decimal places to avoid floating point issues
        amount = std::round(amount * 100.0) / 100.0;

        // Check if amount is multiple of contract size
        double remainder = std::fmod(amount, contract_size);
        if (remainder > 0.0001)
        { // Using small epsilon for floating point comparison
            std::cerr << "Error: Amount " << amount << " is not a multiple of contract size " << contract_size << std::endl;
            std::cerr << "Remainder: " << remainder << std::endl;
            return;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Warning: Could not fetch instrument details: " << e.what() << std::endl;
        std::cout << "Enter amount: ";
        std::cin >> amount;
        clearInputBuffer();
    }

    std::cout << "Enter side (buy/sell): ";
    std::getline(std::cin, side);

    std::cout << "Enter order type (limit/market): ";
    std::getline(std::cin, type);

    if (type == "limit")
    {
        std::cout << "Enter price: ";
        std::cin >> price;
        clearInputBuffer();
    }

    try
    {
        bool success = order_manager->placeOrder(instrument, side, amount, type, price);
        if (success)
        {
            if (type == "market")
            {
                std::cout << "\n=== Market Order Executed ===\n";
                std::cout << "Market orders execute immediately and cannot be cancelled.\n";
            }
            else if (type == "limit")
            {
                std::cout << "\n=== Limit Order Placed ===\n";
                std::cout << "Order ID: " << order_manager->getLastOrderId() << "\n";
                std::cout << "IMPORTANT: Save this Order ID to cancel or modify the order later.\n";
                std::cout << "Use 'View Active Orders' to see all your active limit orders.\n";
            }
        }
        else
        {
            std::cout << "Failed to place order." << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error placing order: " << e.what() << std::endl;
    }
}

void cancelOrder(std::shared_ptr<goquant::DeribitClient> client, std::shared_ptr<goquant::OrderManager> order_manager)
{
    std::cout << "\n=== Cancel Order ===\n";

    // First display active orders
    displayActiveOrders(order_manager);

    const auto &active_orders = order_manager->getActiveOrders();
    if (active_orders.empty())
    {
        std::cout << "No active orders to cancel.\n";
        return;
    }

    std::string order_id;
    std::cout << "\nEnter the complete Order ID to cancel: ";
    std::getline(std::cin, order_id);

    // Validate if order exists
    if (active_orders.find(order_id) == active_orders.end())
    {
        std::cout << "Error: Order ID '" << order_id << "' not found in active orders.\n";
        std::cout << "Please make sure to enter the complete Order ID as shown above.\n";
        return;
    }

    try
    {
        bool success = order_manager->cancelOrder(order_id);
        if (success)
        {
            std::cout << "Successfully cancelled order: " << order_id << "\n";

            // Show remaining active orders
            std::cout << "\nRemaining active orders:\n";
            displayActiveOrders(order_manager);
        }
        else
        {
            std::cout << "Failed to cancel order: " << order_id << "\n";
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error cancelling order: " << e.what() << "\n";
        std::cout << "Please verify the Order ID and try again.\n";
    }
}

void modifyOrder(std::shared_ptr<goquant::DeribitClient> client, std::shared_ptr<goquant::OrderManager> order_manager)
{
    std::string order_id, instrument;
    double new_price, new_amount;

    std::cout << "\n=== Modify Order ===" << std::endl;

    // Show current active orders
    displayActiveOrders(order_manager);

    if (order_manager->getActiveOrders().empty())
    {
        std::cout << "No active orders to modify." << std::endl;
        return;
    }

    std::cout << "\nEnter order ID: ";
    std::getline(std::cin, order_id);

    // Validate order exists
    if (order_manager->getActiveOrders().find(order_id) == order_manager->getActiveOrders().end())
    {
        std::cout << "Error: Order ID not found in active orders." << std::endl;
        return;
    }

    std::cout << "Enter instrument: ";
    std::getline(std::cin, instrument);

    std::cout << "Enter new price: ";
    std::cin >> new_price;
    clearInputBuffer();

    std::cout << "Enter new amount: ";
    std::cin >> new_amount;
    clearInputBuffer();

    try
    {
        if (order_manager->modifyOrder(order_id, new_amount, new_price))
        {
            std::cout << "\n╔═══════════════════════════════════════════╗" << std::endl;
            std::cout << "║          ORDER MODIFIED SUCCESSFULLY        ║" << std::endl;
            std::cout << "╠═══════════════════════════════════════════╣" << std::endl;
            std::cout << "║ Order ID:   " << std::left << std::setw(27) << order_id << "║" << std::endl;
            std::cout << "║ New Amount: " << std::left << std::setw(27) << new_amount << "║" << std::endl;
            std::cout << "║ New Price:  $" << std::left << std::setw(26) << new_price << "║" << std::endl;
            std::cout << "╚═══════════════════════════════════════════╝" << std::endl;

            // Show updated active orders
            std::cout << "\nUpdated Active Orders:" << std::endl;
            displayActiveOrders(order_manager);
        }
        else
        {
            std::cout << "\n╔═══════════════════════════════════════════╗" << std::endl;
            std::cout << "║             MODIFICATION FAILED             ║" << std::endl;
            std::cout << "╚═══════════════════════════════════════════╝" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "\n╔═══════════════════════════════════════════╗" << std::endl;
        std::cout << "║              MODIFICATION ERROR             ║" << std::endl;
        std::cout << "║ " << e.what() << std::endl;
        std::cout << "╚═══════════════════════════════════════════╝" << std::endl;
    }
}

void getOrderbook(std::shared_ptr<goquant::DeribitClient> client)
{
    std::string instrument;
    int depth;

    std::cout << "\nEnter instrument (e.g., BTC-PERPETUAL): ";
    std::getline(std::cin, instrument);

    std::cout << "Enter depth (1-100): ";
    std::cin >> depth;
    clearInputBuffer();

    try
    {
        auto orderbook = client->getOrderbook(instrument, depth);
        auto &result = orderbook["result"];

        std::cout << "\n╔═══════════════════════════════════════════════╗\n";
        std::cout << "║             ORDERBOOK: " << std::left << std::setw(16)
                  << instrument << "║\n";
        std::cout << "╠═══════════════════════════════════════════════╣\n";

        // Market Overview
        std::cout << "║ Last Price:    $" << std::fixed << std::setprecision(2)
                  << std::setw(25) << result["last_price"].get<double>() << "║\n";
        std::cout << "║ Best Bid:      $" << std::setw(25)
                  << result["best_bid_price"].get<double>() << "║\n";
        std::cout << "║ Best Ask:      $" << std::setw(25)
                  << result["best_ask_price"].get<double>() << "║\n";
        std::cout << "╠═══════════════════════════════════════════════╣\n";

        // Sell Orders (Asks)
        std::cout << "║ SELL ORDERS (ASKS)                           ║\n";
        std::cout << "╟───────────┬───────────┬───────────────────╢\n";
        std::cout << "║   Price   │   Size    │     Total USD     ║\n";
        std::cout << "╟───────────┼───────────┼───────────────────╢\n";

        auto asks = result["asks"];
        for (size_t i = 0; i < std::min(5UL, asks.size()); ++i)
        {
            double price = asks[i][0].get<double>();
            double size = asks[i][1].get<double>();
            double total = price * size;

            std::cout << "║ $" << std::setw(8) << price
                      << " │ " << std::setw(8) << size
                      << " │ $" << std::setw(13) << total << " ║\n";
        }

        // Buy Orders (Bids)
        std::cout << "╟───────────┴───────────┴───────────────────╢\n";
        std::cout << "║ BUY ORDERS (BIDS)                           ║\n";
        std::cout << "╟───────────┬───────────┬───────────────────╢\n";
        std::cout << "║   Price   │   Size    │     Total USD     ║\n";
        std::cout << "╟───────────┼───────────┼───────────────────╢\n";

        auto bids = result["bids"];
        for (size_t i = 0; i < std::min(5UL, bids.size()); ++i)
        {
            double price = bids[i][0].get<double>();
            double size = bids[i][1].get<double>();
            double total = price * size;

            std::cout << "║ $" << std::setw(8) << price
                      << " │ " << std::setw(8) << size
                      << " │ $" << std::setw(13) << total << " ║\n";
        }

        std::cout << "╚═══════════════════════════════════════════════╝\n";
    }
    catch (const std::exception &e)
    {
        std::cout << "\n╔══════════════════════════════════╗";
        std::cout << "\n║   Error: Cannot Load Orderbook   ║";
        std::cout << "\n╚══════════════════════════════════╝\n";
    }
}

void viewPositions(std::shared_ptr<goquant::DeribitClient> client)
{
    try
    {
        auto positions = client->getPositions();

        if (!positions.contains("result") || positions["result"].empty())
        {
            std::cout << "\n╔══════════════════════════════════╗";
            std::cout << "\n║      No Open Positions          ║";
            std::cout << "\n╚══════════════════════════════════╝\n";
            return;
        }

        std::cout << "\n╔══════════════════════════════════════════════╗";
        std::cout << "\n║              CURRENT POSITIONS               ║";
        std::cout << "\n╠══════════════════════════════════════════════╣\n";

        for (const auto &pos : positions["result"])
        {
            std::cout << "║ Instrument:     " << std::left << std::setw(28)
                      << pos["instrument_name"].get<std::string>() << "║\n";

            std::cout << "║ Position Size:  " << std::left << std::setw(28)
                      << (std::to_string(pos["size"].get<double>()) + " contracts") << "║\n";

            std::cout << "║ Direction:      " << std::left << std::setw(28)
                      << pos["direction"].get<std::string>() << "║\n";

            std::cout << "║ Average Price:  $" << std::left << std::fixed << std::setprecision(2)
                      << std::setw(27) << pos["average_price"].get<double>() << "║\n";

            std::cout << "║ Mark Price:     $" << std::left << std::fixed
                      << std::setw(27) << pos["mark_price"].get<double>() << "║\n";

            double pnl = pos["floating_profit_loss"].get<double>();
            std::string pnl_str = (pnl >= 0 ? "+" : "") + std::to_string(pnl) + " BTC";
            std::cout << "║ Unrealized PnL: " << std::left << std::setw(28) << pnl_str << "║\n";

            std::cout << "║ Leverage:       " << std::left << std::setw(28)
                      << (std::to_string(pos["leverage"].get<int>()) + "x") << "║\n";

            std::cout << "╠══════════════════════════════════════════════╣\n";
        }
        std::cout << "╚══════════════════════════════════════════════╝\n";
    }
    catch (const std::exception &e)
    {
        std::cout << "\n╔══════════════════════════════════╗";
        std::cout << "\n║      Error: Cannot Load Data     ║";
        std::cout << "\n╚══════════════════════════════════╝\n";
    }
}

void onWebSocketMessage(const std::string &message)
{
    try
    {
        auto json = nlohmann::json::parse(message);

        if (json.contains("params") && json["params"].contains("data"))
        {
            auto &data = json["params"]["data"];

            // Clear previous line
            std::cout << "\033[2K\r"; // Clear line and return cursor to start

            std::cout << "║ Mark: $" << std::fixed << std::setprecision(2)
                      << std::setw(9) << data["mark_price"].get<double>()
                      << " │ Bid: $" << std::setw(9) << data["best_bid_price"].get<double>()
                      << " │ Ask: $" << std::setw(9) << data["best_ask_price"].get<double>()
                      << " ║\r" << std::flush;
        }
    }
    catch (const std::exception &)
    {
        // Silently ignore non-market data messages
    }
}

void realTimeMarketData(std::shared_ptr<goquant::DeribitClient> client)
{
    std::string instrument;
    std::cout << "\nEnter instrument (e.g., BTC-PERPETUAL): ";
    std::getline(std::cin, instrument);

    auto ws = std::make_shared<goquant::WebSocketServer>();
    ws->setMessageCallback(onWebSocketMessage);

    if (ws->connect("test.deribit.com", "443"))
    {
        std::cout << "\n╔══════════════════════════════════════════════╗\n";
        std::cout << "║            REAL-TIME MARKET DATA             ║\n";
        std::cout << "╠══════════════════════════════════════════════╣\n";

        ws->subscribe("ticker", instrument);

        std::cout << "║ Streaming data for: " << std::left << std::setw(24)
                  << instrument << "║\n";
        std::cout << "╠══════════════════════════════════════════════╣\n";
        std::cout << "║ Press Enter to stop streaming...             ║\n";
        std::cout << "╚══════════════════════════════════════════════╝\n\n";

        std::cin.get();

        ws->unsubscribe("ticker", instrument);
        ws->disconnect();
    }
    else
    {
        std::cout << "\n╔══════════════════════════════════╗";
        std::cout << "\n║   Error: Connection Failed       ║";
        std::cout << "\n╚══════════════════════════════════╝\n";
    }
}

void runPerformanceTest(std::shared_ptr<goquant::DeribitClient> client,
                       std::shared_ptr<goquant::OrderManager> order_manager)
{
    std::cout << "\nRunning Performance Tests..." << std::endl;
    auto& monitor = goquant::PerformanceMonitor::getInstance();

    // Test order placement latency
    for (int i = 0; i < 5; i++) {
        monitor.startOperation("order_placement");
        try {
            order_manager->placeOrder("BTC-PERPETUAL", "buy", 10.0, "limit", 50000.0);
        }
        catch (const std::exception& e) {
            spdlog::error("Test order placement failed: {}", e.what());
        }
        monitor.endOperation("order_placement");
        
        // Add small delay between tests
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Test market data latency
    monitor.startOperation("market_data");
    try {
        auto orderbook = client->getOrderbook("BTC-PERPETUAL", 10);
    }
    catch (const std::exception& e) {
        spdlog::error("Market data test failed: {}", e.what());
    }
    monitor.endOperation("market_data");

    // Print performance report
    goquant::PerformanceAnalyzer::printLatencyReport();
}

int main()
{
    auto& monitor = goquant::PerformanceMonitor::getInstance();
    
    // Create thread pool
    goquant::ThreadPool pool(std::thread::hardware_concurrency());
    
    std::string client_id, client_secret;

    std::cout << "=== Deribit Trading System Login ===" << std::endl;
    std::cout << "Enter your Client ID: ";
    std::getline(std::cin, client_id);

    std::cout << "Enter your Client Secret: ";
    std::getline(std::cin, client_secret);

    // Set environment variables
    setenv("DERIBIT_CLIENT_ID", client_id.c_str(), 1);
    setenv("DERIBIT_CLIENT_SECRET", client_secret.c_str(), 1);

    // Create Deribit client instance
    auto client = std::make_shared<goquant::DeribitClient>();

    // Create OrderManager instance
    auto order_manager = std::make_shared<goquant::OrderManager>(client);

    // Authenticate
    std::cout << "\nAuthenticating..." << std::endl;
    try
    {
        client->authenticate();
        std::cout << "Authentication successful!" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Authentication failed: " << e.what() << std::endl;
        return 1;
    }

    // Main menu loop
    int choice;
    do
    {
        displayMainMenu();
        std::cin >> choice;
        clearInputBuffer();

        switch (choice)
        {
        case 1:
            placeOrder(client, order_manager);
            break;
        case 2:
            cancelOrder(client, order_manager);
            break;
        case 3:
            modifyOrder(client, order_manager);
            break;
        case 4:
            getOrderbook(client);
            break;
        case 5:
            viewPositions(client);
            break;
        case 6:
            realTimeMarketData(client);
            break;
        case 7:
            displayActiveOrders(order_manager);
            break;
        case 8:
            runPerformanceTest(client, order_manager);
            break;
        case 0:
            std::cout << "Exiting..." << std::endl;
            break;
        default:
            std::cout << "Invalid choice. Please try again." << std::endl;
        }
    } while (choice != 0);

    return 0;
}
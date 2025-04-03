#include "deribit_client.hpp"
#include "websocket_server.hpp"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cmath>
#include <string>
#include <limits>

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
    std::cout << "0. Exit" << std::endl;
    std::cout << "Enter your choice: ";
}

void placeOrder(std::shared_ptr<goquant::DeribitClient> client)
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
        auto order = client->placeOrder(instrument, side, amount, type, price);
        std::cout << "Order placed successfully!" << std::endl;
        std::cout << "Order details: " << order.dump(2) << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error placing order: " << e.what() << std::endl;
        std::cout << "Please ensure the amount is a multiple of the contract size." << std::endl;
    }
}

void cancelOrder(std::shared_ptr<goquant::DeribitClient> client)
{
    std::string order_id;

    std::cout << "\n=== Cancel Order ===" << std::endl;
    std::cout << "Enter order ID: ";
    std::getline(std::cin, order_id);

    try
    {
        auto result = client->cancelOrder(order_id);
        std::cout << "Order cancelled successfully!" << std::endl;
        std::cout << "Cancellation details: " << result.dump(2) << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error cancelling order: " << e.what() << std::endl;
    }
}

void modifyOrder(std::shared_ptr<goquant::DeribitClient> client)
{
    std::string order_id, instrument;
    double new_price, new_amount;

    std::cout << "\n=== Modify Order ===" << std::endl;
    std::cout << "Enter order ID: ";
    std::getline(std::cin, order_id);

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
        auto result = client->modifyOrder(order_id, instrument, new_price, new_amount);
        std::cout << "Order modified successfully!" << std::endl;
        std::cout << "Modified order details: " << result.dump(2) << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error modifying order: " << e.what() << std::endl;
    }
}

void getOrderbook(std::shared_ptr<goquant::DeribitClient> client)
{
    std::string instrument;
    int depth;

    std::cout << "\n=== Get Orderbook ===" << std::endl;
    std::cout << "Enter instrument (e.g., BTC-PERPETUAL): ";
    std::getline(std::cin, instrument);

    std::cout << "Enter depth (1-100): ";
    std::cin >> depth;
    clearInputBuffer();

    try
    {
        auto orderbook = client->getOrderbook(instrument, depth);
        std::cout << "Orderbook received successfully!" << std::endl;
        std::cout << "Best bid: " << orderbook["result"]["best_bid_price"].get<double>() << std::endl;
        std::cout << "Best ask: " << orderbook["result"]["best_ask_price"].get<double>() << std::endl;
        std::cout << "Full orderbook: " << orderbook.dump(2) << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error fetching orderbook: " << e.what() << std::endl;
    }
}

void viewPositions(std::shared_ptr<goquant::DeribitClient> client)
{
    std::cout << "\n=== Current Positions ===" << std::endl;
    try
    {
        auto positions = client->getPositions();
        std::cout << "Current positions: " << positions.dump(2) << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error fetching positions: " << e.what() << std::endl;
    }
}

void onWebSocketMessage(const std::string &message)
{
    std::cout << "Received: " << message << std::endl;
}

void realTimeMarketData(std::shared_ptr<goquant::DeribitClient> client)
{
    std::string instrument;
    std::cout << "\n=== Real-time Market Data ===" << std::endl;
    std::cout << "Enter instrument to subscribe (e.g., BTC-PERPETUAL): ";
    std::getline(std::cin, instrument);

    auto ws = std::make_shared<goquant::WebSocketServer>();
    ws->setMessageCallback(onWebSocketMessage);

    if (ws->connect("test.deribit.com", "443"))
    {
        std::cout << "WebSocket connected successfully!" << std::endl;
        std::cout << "Subscribing to " << instrument << " orderbook..." << std::endl;
        ws->subscribe("book", instrument);

        std::cout << "\nPress Enter to stop streaming..." << std::endl;
        std::cin.get();

        std::cout << "Unsubscribing..." << std::endl;
        ws->unsubscribe("book", instrument);
        ws->disconnect();
    }
    else
    {
        std::cerr << "Failed to connect to WebSocket server" << std::endl;
    }
}

int main()
{
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
            placeOrder(client);
            break;
        case 2:
            cancelOrder(client);
            break;
        case 3:
            modifyOrder(client);
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
        case 0:
            std::cout << "Exiting..." << std::endl;
            break;
        default:
            std::cout << "Invalid choice. Please try again." << std::endl;
        }
    } while (choice != 0);

    return 0;
}
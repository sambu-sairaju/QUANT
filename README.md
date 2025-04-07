# GoQuant Trading System

A high-performance order execution and management system for trading on Deribit Test using C++.

## Features

- Order Management
  - Place orders
  - Cancel orders
  - Modify orders
  - View orderbook
  - Track positions
- Real-time Market Data
  - WebSocket streaming
  - Orderbook updates
  - Price tickers
- Support for Multiple Instruments
  - Spot
  - Futures
  - Options

## Requirements

- C++20 compatible compiler
- CMake 3.15 or higher
- OpenSSL
- libcurl
- nlohmann_json
- WebSocket++

## Building from Source

1. Unzip(unzip GoQuant.zip) file and navigate to the folder:
```bash
cd GoQuant
```

2. Create a build directory and navigate to it:
```bash
mkdir build
cd build
```

3. Configure and build the project:
```bash
cmake ..
make
```

## Configuration

1. Create a Deribit Test account at https://test.deribit.com/
2. Generate API keys from your account settings
3. Set up your API credentials in the configuration file

## Usage

1. Initialize the trading system:
```cpp
auto client = std::make_shared<goquant::DeribitClient>(api_key, api_secret);
auto order_manager = std::make_shared<goquant::OrderManager>(client);
auto market_data = std::make_shared<goquant::MarketData>(ws_server);
```

2. Place an order:
```cpp
order_manager->placeOrder("BTC-PERPETUAL", "buy", 0.1, "limit", 50000.0);
```

3. Subscribe to market data:
```cpp
market_data->subscribeOrderBook("BTC-PERPETUAL");
```

## Performance Optimization

The system includes several optimizations for low-latency trading:

- Efficient memory management
- Lock-free data structures where possible
- Optimized network communication
- Thread-safe order management
- Real-time market data processing
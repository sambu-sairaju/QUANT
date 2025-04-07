cmake_minimum_required(VERSION 3.15)
project(GoQuant VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set policy for new Boost find mechanism
cmake_policy(SET CMP0074 NEW)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Set OpenSSL root dir for macOS
if(APPLE)
    set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
    set(OPENSSL_LIBRARIES "/opt/homebrew/opt/openssl@3/lib")
endif()

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

# Set Boost components and minimum version
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.74.0 REQUIRED COMPONENTS system)

# Add source files
set(SOURCES
    src/main.cpp
    src/deribit_client.cpp
    src/websocket_server.cpp
    src/order_manager.cpp
    src/market_data.cpp
    src/utils.cpp
)

# Add header files
set(HEADERS
    include/deribit_client.hpp
    include/websocket_server.hpp
    include/order_manager.hpp
    include/market_data.hpp
    include/utils.hpp
    include/performance_monitor.hpp
    include/performance_analyzer.hpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
        CURL::libcurl
        nlohmann_json::nlohmann_json
        ${Boost_LIBRARIES}
        spdlog::spdlog
)

# Add compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif() 
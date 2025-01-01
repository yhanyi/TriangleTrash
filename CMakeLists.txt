cmake_minimum_required(VERSION 3.15)
project(OrderBook VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
enable_testing()
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG main
)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG main
)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)

set(BENCHMARK_ENABLE_TESTING OFF)
FetchContent_MakeAvailable(googletest benchmark json)

add_library(orderbook_lib
    src/orderbook/orderbook.cpp
    src/orderbook/order.cpp
    src/network/server.cpp)

target_include_directories(orderbook_lib 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(orderbook_lib
    PRIVATE
        nlohmann_json::nlohmann_json)

add_executable(orderbook_server src/main.cpp)
target_link_libraries(orderbook_server PRIVATE orderbook_lib)
target_include_directories(orderbook_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(orderbook_tests
    tests/orderbook_test.cpp
    tests/network_test.cpp)
target_link_libraries(orderbook_tests PRIVATE
    orderbook_lib
    GTest::gtest_main)
add_test(NAME orderbook_tests COMMAND orderbook_tests)

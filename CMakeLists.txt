cmake_minimum_required(VERSION 3.15)
project(TriangleTrash VERSION 1.0)

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

add_library(triangletrash_lib
    src/orderbook/orderbook.cpp
    src/orderbook/order.cpp
    src/network/server.cpp
    src/network/thread_pool.cpp
    src/session/user.cpp
    src/session/session.cpp)

target_include_directories(triangletrash_lib 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(triangletrash_lib
  PUBLIC
        nlohmann_json::nlohmann_json)

add_executable(triangletrash_server src/main.cpp)
target_link_libraries(triangletrash_server PRIVATE triangletrash_lib)
target_include_directories(triangletrash_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(triangletrash_tests
    tests/orderbook_test.cpp
    tests/network_test.cpp)
target_link_libraries(triangletrash_tests PRIVATE
    triangletrash_lib
    GTest::gtest_main
    nlohmann_json::nlohmann_json)
add_test(NAME triangletrash_tests COMMAND triangletrash_tests)

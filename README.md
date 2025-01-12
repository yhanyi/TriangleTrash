# TriangleTrash

C++ implementation of the Delta Dash game that Optiver likes to run during campus recruitment talks. I've already gotten rejected by Optiver this cycle, so I'll just practice trading against myself :(

**Note**: This project is exploratory in nature and was created for fun, with the goal of implementing and integrating as many interesting concepts as possible. It is likely not the most optimized or production-ready project!

## Setup and Build

```bash
# Create and enter build directory
mkdir build && cd build

# Generate build files
cmake ..

# Build the project
make

# Run the server
./triangletrash_server

# Or run tests
./triangletrash_tests
```

## Usage

### Connecting to the Server
- Start the server: `./triangletrash_server`
- Connect as a client: `nc localhost 8080`
- Default trading session is created automatically

### Placing Orders
Send JSON-formatted orders to interact with the order book:
```json
{
    "type": "join",
    "username": "trader1",
    "session_id": "default"
}

{
    "type": "new_order",
    "session_id": "default",
    "side": "buy",
    "price": 100.0,
    "quantity": 10,
    "order_id": 1
}
```

## Attempted Features

- Concurrency

    - Thread-safe orderbook operations
    - Concurrent client handling with connection pooling
    - Lock-free data structures where possible
    - Reader-writer locks

- Performance

    - Minimised lock contention in critical paths
    - Batched order processing
    - Optimised network I/O

- Thread Pool

    - Task scheduling with thread pool
    - Configurable number of worker threads based on hardware
    - Task queue with shared mutex synchronisation
    - Support for both async and immediate execution

- Memory Pool

    - Block-based allocation strategy (with 4KB blocks)
    - Thread-safe with atomic operations and mutex protection
    - Memory recycling through a free list
    - O(1) allocation and deallocation
    - Supports up to 1024 memory blocks
    - Orders are managed through `OrderAllocator` which provides a type-safe interface

- Network Optimisations

    - Zero-copy networking for reduced latency
    - Optimised socket handling

## Notes

- C++20, GoogleTests, GoogleBenchmark
- RAII for resource management
- Smart pointers for memory safety
- CI with GitHub Actions with sanitised build

Thank you for checking out this project! :)

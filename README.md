# TriangleTrash

C++ implementation of the Delta Dash game that Optiver likes to run during campus recruitment talks. I've already gotten rejected by Optiver this cycle, so I'll just practice trading against myself :(

### Setup

- Run `mkdir build && cd build` to create a build folder and navigate into it.
- Run `cmake ..` to build the project into the folder. Then run `make` to compile the code into executable binaries.
- Run either `./triangletrash_server` to start the server and test locally, or `./triangletrash_tests` to run the test suite.

### Local Testing

- Run `./triangletrash_server` in a terminal to start the server. Then run `nc localhost 8080` in other terminal instances to connect to the server.
- Copy and paste json data of orders to interact with the orderbook on the server. This functionality and user interface will be gradually iterated on.
```bash
{{"type", "new_order"}, {"session_id", "test_session"},
                   {"side", "buy"}, {"price", 90.0},
                   {"quantity", 10}, {"order_id", 0}}
```

~~To compile, run `make`. In a terminal instance, run `./triangletrash`. Then on separate terminal instances, run `nc localhost 8080` to connect to the game server.~~

Have fun!

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -I.
LDFLAGS = -pthread

# For Mac with Apple Silicon chips.
CXXFLAGS += -arch arm64
LDFLAGS += -arch arm64

TARGET = triangletrash
SRCS = main.cpp server.cpp orderbook.cpp game.cpp player.cpp order.cpp option.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = game.hpp player.hpp order.hpp orderbook.hpp server.hpp

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
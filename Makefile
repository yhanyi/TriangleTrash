CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -I.
LDFLAGS = -pthread

CXXFLAGS += -arch arm64
LDFLAGS += -arch arm64

TARGET = triangletrash
SRCS = main.cpp server.cpp orderbook.cpp game.cpp player.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = game.hpp player.hpp order.hpp

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall -mavx2 -msse3
LDFLAGS = -lmetis -lpthread

# Target and sources
TARGET = reorder
SRCS = reorder.cpp
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o

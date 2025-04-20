# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall -mavx2 -msse3
LDFLAGS = -lroaring -lpthread

# Target and sources
TARGET = tc
SOURCES = tc.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Pattern rule for .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(TARGET) *.o

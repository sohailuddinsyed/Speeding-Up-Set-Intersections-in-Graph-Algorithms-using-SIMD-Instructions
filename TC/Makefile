# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -mavx2 -msse3 -Iinclude

LDFLAGS = -lpthread

# Target binary name
TARGET = triangle_exec_driver

# Source files
SOURCES = triangle_exec_driver.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Default build rule
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile each .cpp to .o with headers in include/
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(TARGET) *.o

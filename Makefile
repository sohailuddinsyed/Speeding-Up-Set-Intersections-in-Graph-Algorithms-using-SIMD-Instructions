# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall -mavx2 -msse3
LDFLAGS = -lpthread

# Target binary name
TARGET = triangle_exec_driver

# Source files (ignore reorder and data folders)
SOURCES = triangle_exec_driver.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Default build rule
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile each .cpp to .o, include correct headers
%.o: %.cpp graph_data_utils.hpp vectorized_set_operations.hpp simd_layout_counter.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(TARGET) *.o

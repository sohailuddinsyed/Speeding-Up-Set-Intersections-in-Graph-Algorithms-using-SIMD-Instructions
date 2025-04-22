# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wno-unused-result -mavx2 -msse3 -Iinclude -I./roaring

LDFLAGS = -lpthread

# Target binary name
TARGET = MainCliqueDetection

# Source files
SOURCES = MainCliqueDetection.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Default build rule
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile each .cpp to .o with headers in include/
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile roaring library
roaring.o: roaring/roaring.c roaring/roaring.h roaring/roaring.hh
	$(CXX) $(CXXFLAGS) -c roaring/roaring.c -o $@

# Clean up build artifacts
clean:
	rm -f $(TARGET) *.o

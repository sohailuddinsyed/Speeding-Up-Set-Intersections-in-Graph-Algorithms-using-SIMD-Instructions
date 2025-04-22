CXX = g++ -std=c++17
CXXFLAGS = -O3 -Wall -Wno-unused-result -mavx2 -pthread -I./roaring

TARGET = MainCliqueDetection
SOURCES = MainCliqueDetection.cpp SIMDSetIntersection.hpp
HEADERS = GraphUtils.hpp SIMDSetIntersection.hpp SIMDMaximalCliqueFinder.hpp NaiveMaximalCliqueFinder.hpp 
OBJECTS = $(SOURCES:.cpp=.o) 

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

roaring.o: roaring/roaring.c roaring/roaring.h roaring/roaring.hh
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) 
CXX = g++ -std=c++11
CXXFLAGS = -O3 -Wall -Wno-unused-result -mavx2 -pthread -I./roaring

TARGET = mc
SOURCES = mc.cpp set_operation.cpp
HEADERS = util.hpp set_operation.hpp bitpack_maximal_clique.hpp org_maximal_clique.hpp roaring_maximal_clique.hpp
OBJECTS = $(SOURCES:.cpp=.o) roaring.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

roaring.o: roaring/roaring.c roaring/roaring.h roaring/roaring.hh
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

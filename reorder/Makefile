# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall -mavx2 -msse3
LDFLAGS = -lmetis -lpthread

# File and directory structure
TARGET = layoutopt
SRCDIR = .
INCDIR = .
SRCS = $(SRCDIR)/gro_driver.cpp
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -f $(TARGET) *.o

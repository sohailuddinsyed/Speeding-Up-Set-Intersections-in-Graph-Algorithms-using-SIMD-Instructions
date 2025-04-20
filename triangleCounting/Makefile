# Compiler and flags
CC = gcc
CFLAGS = -O3 -Wall -march=native -std=gnu11

# Files
SRCS = tc.c
OBJS = $(SRCS:.c=.o)
EXEC = triangle_counter

all: $(EXEC)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(EXEC) *.o

.PHONY: all clean
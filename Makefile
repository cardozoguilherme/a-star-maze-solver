CC = gcc
CFLAGS = -Ofast -march=native -mtune=native -flto -ffast-math -funroll-loops \
         -fomit-frame-pointer -fprefetch-loop-arrays -fno-stack-protector \
         -fno-exceptions -fno-asynchronous-unwind-tables \
         -Wall -Wextra -Wpedantic
LDFLAGS = -flto -s

SRC = src/maze_solver.c
TARGET = maze_solver

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET) output.txt path.json

.PHONY: all clean
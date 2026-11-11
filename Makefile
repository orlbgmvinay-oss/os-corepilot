# CampusCompute - build with:  make        (needs gcc and make on Linux/Ubuntu/WSL)
CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2 -pthread -Iinclude
LDLIBS  = -pthread -lrt -lm
TARGET  = campuscompute
SRCS    = $(wildcard src/*.c)
OBJS    = $(SRCS:src/%.c=build/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

build/%.o: src/%.c $(wildcard include/*.h) | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

test: $(TARGET)
	bash tests/run_tests.sh

demo: $(TARGET)
	./$(TARGET) demo

# Removes the program's own build output and IPC objects (nothing else).
clean:
	rm -rf build $(TARGET)
	-@[ -x ./$(TARGET) ] && ./$(TARGET) cleanup >/dev/null 2>&1 || true

.PHONY: all test demo clean

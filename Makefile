# ==========================================
# CampusCompute - Operating Systems Project
# ==========================================

CC = gcc

CFLAGS = -Wall -Wextra -Iinclude -g

LDFLAGS = -pthread -lrt

TARGET = campus_compute

SRC = src/main.c \
      src/common.c \
      src/job.c \
      src/scheduler.c \
      src/logger.c \
      src/ipc.c \
      src/banker.c \
      src/worker.c \
      src/resource_limits.c

OBJ = $(SRC:.c=.o)

# Default target
all: $(TARGET)

# Build main executable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run the application
run: $(TARGET)
	./$(TARGET)

# Run individual tests
test_logger:
	$(CC) $(CFLAGS) src/logger.c tests/test_logger.c -o test_logger $(LDFLAGS)
	./test_logger

test_ipc:
	$(CC) $(CFLAGS) src/ipc.c tests/test_ipc.c -o test_ipc $(LDFLAGS)
	./test_ipc

test_worker:
	$(CC) $(CFLAGS) src/worker.c tests/test_worker.c -o test_worker $(LDFLAGS)
	./test_worker

test_resource_limits:
	$(CC) $(CFLAGS) src/resource_limits.c tests/test_resource_limits.c -o test_resource_limits
	./test_resource_limits

test_scheduler:
	$(CC) $(CFLAGS) src/common.c src/job.c src/scheduler.c tests/test_scheduler.c -o test_scheduler $(LDFLAGS)
	./test_scheduler

test_banker:
	$(CC) $(CFLAGS) src/banker.c tests/test_banker.c -o test_banker $(LDFLAGS)
	./test_banker

# Remove generated files
clean:
	rm -f $(OBJ) $(TARGET) \
	      test_logger test_ipc test_worker \
	      test_resource_limits test_scheduler test_banker

.PHONY: all run clean test_logger test_ipc test_worker \
        test_resource_limits test_scheduler test_banker

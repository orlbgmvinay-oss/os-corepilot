CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS = -Iinclude
THREAD_FLAGS = -pthread

.PHONY: all test clean
all: campus_compute

SOURCES = src/main.o src/common.o src/banker.o src/ipc.o src/job.o src/logger.o \
	src/resource_limits.o src/scheduler.o src/worker.o

campus_compute: $(SOURCES)
	$(CC) $(CFLAGS) $(THREAD_FLAGS) -o $@ $^

test: tests/test_job tests/test_banker test_ipc test_logger test_resource_limits \
        tests/test_scheduler test_worker
	./tests/test_job
	./tests/test_banker
	./test_ipc
	./test_logger
	./test_resource_limits
	./tests/test_scheduler
	./test_worker

tests/test_job: tests/test_job.c src/job.c include/job.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_job.c src/job.c

tests/test_banker: tests/test_banker.c src/banker.c include/banker.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_banker.c src/banker.c

test_ipc: tests/test_ipc.c src/ipc.c include/ipc.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_ipc.c src/ipc.c

test_logger: tests/test_logger.c src/logger.c include/logger.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(THREAD_FLAGS) -o $@ tests/test_logger.c src/logger.c

test_resource_limits: tests/test_resource_limits.c src/resource_limits.c include/resource_limits.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_resource_limits.c src/resource_limits.c

tests/test_scheduler: tests/test_scheduler.c src/job.c src/scheduler.c include/job.h include/scheduler.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_scheduler.c src/job.c src/scheduler.c

test_worker: tests/test_worker.c src/worker.c include/worker.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_worker.c src/worker.c

src/main.o: src/main.c include/banker.h include/common.h include/ipc.h include/job.h include/logger.h include/resource_limits.h include/scheduler.h include/worker.h
src/common.o: src/common.c include/common.h
src/job.o: src/job.c include/job.h
src/scheduler.o: src/scheduler.c include/scheduler.h include/job.h
src/banker.o: src/banker.c include/banker.h
src/ipc.o: src/ipc.c include/ipc.h
src/logger.o: src/logger.c include/logger.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(THREAD_FLAGS) -c -o $@ $<
src/resource_limits.o: src/resource_limits.c include/resource_limits.h
src/worker.o: src/worker.c include/worker.h

clean:
	rm -f src/*.o campus_compute tests/test_job tests/test_banker tests/test_scheduler \
		test_ipc test_logger test_resource_limits test_worker

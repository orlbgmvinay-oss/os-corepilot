CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS = -Iinclude

.PHONY: all clean
all: member1_demo

member1_demo: src/demo_main.o src/job.o src/scheduler.o src/banker.o
	$(CC) $(CFLAGS) -o $@ $^

src/demo_main.o: src/demo_main.c include/job.h include/scheduler.h include/banker.h
src/job.o: src/job.c include/job.h
src/scheduler.o: src/scheduler.c include/scheduler.h include/job.h
src/banker.o: src/banker.c include/banker.h

clean:
	rm -f src/*.o member1_demo

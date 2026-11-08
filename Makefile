CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

.PHONY: all clean
all: member1_demo

member1_demo: demo_main.o job.o scheduler.o banker.o
	$(CC) $(CFLAGS) -o $@ $^

demo_main.o: demo_main.c job.h scheduler.h banker.h
job.o: job.c job.h
scheduler.o: scheduler.c scheduler.h job.h
banker.o: banker.c banker.h

clean:
	rm -f *.o member1_demo

#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "job.h"

/* Scheduling output is printed to stdout. Lower priority value = higher priority. */
void run_fcfs(const Job jobs[], int count);
void run_round_robin(const Job jobs[], int count, int time_quantum);
void run_priority(const Job jobs[], int count);
void compare_schedulers(const Job jobs[], int count, int time_quantum);

#endif

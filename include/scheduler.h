/*
 * scheduler.h - FR2: FCFS, Round Robin, non-preemptive Priority (+ optional aging).
 */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "job.h"

typedef enum { POLICY_FCFS, POLICY_RR, POLICY_PRIORITY } Policy;

/* Result of running one policy: per-job metrics plus averages. */
typedef struct {
    Job    jobs[MAX_JOBS];
    int    n;
    double avg_wt, avg_tat;
} SchedResult;

/* Run one policy on a copy of `set` (used by the IPC demo). Returns 0 on success. */
int sched_run(Policy p, const JobSet *set, int quantum, SchedResult *out);
const char *policy_short(Policy p);
int policy_parse(const char *s, Policy *out);

/* argv[0] = fcfs | rr | priority | compare, argv[1] = job file, then options. */
int run_scheduler(int argc, char **argv);

#endif /* SCHEDULER_H */

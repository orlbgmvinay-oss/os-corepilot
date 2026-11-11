/*
 * job.h - FR1: job representation, validation and lifecycle state.
 */
#ifndef JOB_H
#define JOB_H

#include "common.h"

/* Lifecycle: NEW -> READY (arrived) -> RUNNING -> DONE */
typedef enum { JOB_NEW, JOB_READY, JOB_RUNNING, JOB_DONE } JobStatus;

typedef struct {
    int  id;
    char name[24];
    int  arrival;      /* time the job enters the system            */
    int  burst;        /* total CPU time required                   */
    int  priority;     /* 1 = most urgent ... 99 = least urgent     */
    JobStatus status;

    /* filled in by the simulators */
    int remaining;
    int first_start;   /* first time the job got the CPU            */
    int completion;
    int turnaround;
    int waiting;
    int response;
} Job;

typedef struct {
    Job jobs[MAX_JOBS];
    int count;
} JobSet;

/* Load and validate a job file. Returns 0, or -1 with a message in err. */
int  job_load(const char *path, JobSet *js, char *err, size_t errlen);
/* Convenience wrapper: prints "error: ..." itself and returns -1 on failure. */
int  jobset_load(const char *path, JobSet *js);
void job_reset_runtime(Job *j);
const char *job_status_str(JobStatus s);
void job_print_input(const JobSet *js, const char *path);

/* `gen <count> <seed> <outfile>` - write a reproducible random job file. */
int  run_gen(int argc, char **argv);

#endif /* JOB_H */

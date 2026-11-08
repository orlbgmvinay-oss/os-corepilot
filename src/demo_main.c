/* Standalone test driver for Member 1 modules. The team can replace this with the final main.c. */
#include "job.h"
#include "scheduler.h"
#include "banker.h"
#include <stdio.h>

int main(void) {
    Job jobs[MAX_JOBS];
    int count = 0;
    add_job(jobs, &count, "J1", 0, 5, 2);
    add_job(jobs, &count, "J2", 1, 3, 1);
    add_job(jobs, &count, "J3", 2, 8, 3);
    add_job(jobs, &count, "J4", 3, 4, 2);

    display_jobs(jobs, count);
    compare_schedulers(jobs, count, 2);
    banker_demo();
    return 0;
}

#include "job.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    Job jobs[MAX_JOBS];
    int count = 0;

    /* Test 1: Add a valid job */
    assert(add_job(jobs, &count, "J1", 0, 5, 2));
    assert(count == 1);
    printf("Test 1 passed: Valid job added.\n");

    /* Test 2: Reject duplicate job ID */
    assert(!add_job(jobs, &count, "J1", 1, 3, 1));
    assert(count == 1);
    printf("Test 2 passed: Duplicate job rejected.\n");

    /* Test 3: Reject empty job ID */
    assert(!add_job(jobs, &count, "", 0, 5, 1));
    printf("Test 3 passed: Empty job ID rejected.\n");

    /* Test 4: Reject negative arrival time */
    assert(!add_job(jobs, &count, "J2", -1, 5, 1));
    printf("Test 4 passed: Negative arrival time rejected.\n");

    /* Test 5: Reject zero CPU burst */
    assert(!add_job(jobs, &count, "J2", 0, 0, 1));
    printf("Test 5 passed: Zero CPU burst rejected.\n");

    /* Test 6: Reject negative priority */
    assert(!add_job(jobs, &count, "J2", 0, 5, -1));
    printf("Test 6 passed: Negative priority rejected.\n");

    /* Test 7: Update job status */
    assert(update_job_status(jobs, count, "J1", JOB_RUNNING));
    assert(jobs[0].status == JOB_RUNNING);
    printf("Test 7 passed: Job status changed to Running.\n");

    /* Test 8: Complete job */
    assert(update_job_status(jobs, count, "J1", JOB_COMPLETED));
    assert(jobs[0].status == JOB_COMPLETED);
    printf("Test 8 passed: Job status changed to Completed.\n");

    /* Test 9: Reject nonexistent job */
    assert(!update_job_status(jobs, count, "J99", JOB_COMPLETED));
    printf("Test 9 passed: Nonexistent job rejected.\n");

    printf("\nAll FR1 job-management tests passed!\n");

    return 0;
}

#define _POSIX_C_SOURCE 200809L

#include "job.h"
#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void check(int condition, const char *expression, const char *file, int line)
{
    if (!condition)
    {
        fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
        exit(EXIT_FAILURE);
    }
}

#define CHECK(expression) check((expression), #expression, __FILE__, __LINE__)

typedef void (*SchedulerCall)(const Job jobs[], int count);

static void call_round_robin(const Job jobs[], int count)
{
    run_round_robin(jobs, count, 2);
}

static void capture_output(SchedulerCall run, const Job jobs[], int count,
                           char output[], size_t output_size)
{
    FILE *capture = tmpfile();
    CHECK(capture != NULL);

    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    CHECK(saved_stdout >= 0);
    CHECK(dup2(fileno(capture), STDOUT_FILENO) >= 0);

    run(jobs, count);
    fflush(stdout);

    CHECK(dup2(saved_stdout, STDOUT_FILENO) >= 0);
    close(saved_stdout);
    rewind(capture);
    size_t read_size = fread(output, 1, output_size - 1, capture);
    output[read_size] = '\0';
    fclose(capture);
}

int main(void)
{
    Job jobs[MAX_JOBS];
    int count = 0;
    char output[4096];

    CHECK(add_job(jobs, &count, "A", 2, 3, 2) == 1);
    CHECK(add_job(jobs, &count, "B", 0, 4, 1) == 1);
    CHECK(add_job(jobs, &count, "C", 2, 1, 3) == 1);
    CHECK(find_job(jobs, count, "B") == 1);

    capture_output(run_fcfs, jobs, count, output, sizeof(output));
    CHECK(strstr(output, "[B: 0-4] [A: 4-7] [C: 7-8]") != NULL);
    CHECK(strstr(output, "Average turnaround time: 5.00") != NULL);
    CHECK(strstr(output, "Average waiting time:    2.33") != NULL);

    Job priority_jobs[MAX_JOBS];
    int priority_count = 0;
    CHECK(add_job(priority_jobs, &priority_count, "A", 0, 5, 2) == 1);
    CHECK(add_job(priority_jobs, &priority_count, "B", 0, 2, 1) == 1);
    CHECK(add_job(priority_jobs, &priority_count, "C", 1, 1, 0) == 1);

    capture_output(run_priority, priority_jobs, priority_count,
                   output, sizeof(output));
    CHECK(strstr(output, "[B: 0-2] [C: 2-3] [A: 3-8]") != NULL);

    Job rr_jobs[MAX_JOBS];
    int rr_count = 0;
    CHECK(add_job(rr_jobs, &rr_count, "A", 0, 3, 1) == 1);
    CHECK(add_job(rr_jobs, &rr_count, "B", 1, 2, 1) == 1);

    capture_output(call_round_robin, rr_jobs, rr_count, output, sizeof(output));
    CHECK(strstr(output, "[A: 0-2] [B: 2-4] [A: 4-5]") != NULL);

    puts("Scheduler tests passed.");
    return 0;
}

#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SEGMENTS 10000

typedef struct { int job_index, start, end; } Segment;
typedef struct { int completion, turnaround, waiting; } Metrics;

static int valid_jobs(const Job jobs[], int n) {
    if (!jobs || n <= 0 || n > MAX_JOBS) {
        printf("Error: provide between 1 and %d jobs.\n", MAX_JOBS);
        return 0;
    }
    for (int i = 0; i < n; ++i) {
        if (jobs[i].arrival_time < 0 || jobs[i].cpu_burst <= 0 || jobs[i].priority < 0) {
            printf("Error: job %s has invalid scheduling values.\n", jobs[i].id);
            return 0;
        }
    }
    return 1;
}

static int earlier_job(const Job jobs[], int a, int b) {
    if (jobs[a].arrival_time != jobs[b].arrival_time)
        return jobs[a].arrival_time < jobs[b].arrival_time;
    return a < b; /* Stable input-order tie break. */
}

static void print_results(const char *name, const Job jobs[], int n,
                          const Metrics m[], const Segment seg[], int seg_count) {
    double total_tat = 0, total_wt = 0;
    printf("\n=== %s ===\n", name);
    printf("Execution timeline: ");
    for (int i = 0; i < seg_count; ++i)
        printf("[%s: %d-%d] ", jobs[seg[i].job_index].id, seg[i].start, seg[i].end);
    printf("\n\n%-12s %-12s %-15s %-12s %-12s\n",
           "Job", "Completion", "Turnaround", "Waiting", "Arrival");
    for (int i = 0; i < n; ++i) {
        printf("%-12s %-12d %-15d %-12d %-12d\n", jobs[i].id,
               m[i].completion, m[i].turnaround, m[i].waiting, jobs[i].arrival_time);
        total_tat += m[i].turnaround;
        total_wt += m[i].waiting;
    }
    printf("Average turnaround time: %.2f\n", total_tat / n);
    printf("Average waiting time:    %.2f\n", total_wt / n);
}

void run_fcfs(const Job jobs[], int n) {
    if (!valid_jobs(jobs, n)) return;
    int order[MAX_JOBS];
    Metrics m[MAX_JOBS] = {{0}};
    Segment seg[MAX_JOBS];
    for (int i = 0; i < n; ++i) order[i] = i;
    for (int i = 0; i < n - 1; ++i)
        for (int j = i + 1; j < n; ++j)
            if (earlier_job(jobs, order[j], order[i])) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
    int time = 0;
    for (int k = 0; k < n; ++k) {
        int x = order[k];
        if (time < jobs[x].arrival_time) time = jobs[x].arrival_time;
        seg[k] = (Segment){x, time, time + jobs[x].cpu_burst};
        time += jobs[x].cpu_burst;
        m[x].completion = time;
        m[x].turnaround = time - jobs[x].arrival_time;
        m[x].waiting = m[x].turnaround - jobs[x].cpu_burst;
    }
    print_results("First Come First Serve (FCFS)", jobs, n, m, seg, n);
    printf("Tie-break: earlier arrival, then original input order.\n");
}

void run_priority(const Job jobs[], int n) {
    if (!valid_jobs(jobs, n)) return;
    int done[MAX_JOBS] = {0}, completed = 0, time = 0, sc = 0;
    Metrics m[MAX_JOBS] = {{0}};
    Segment seg[MAX_JOBS];
    while (completed < n) {
        int best = -1;
        for (int i = 0; i < n; ++i) {
            if (!done[i] && jobs[i].arrival_time <= time) {
                if (best < 0 || jobs[i].priority < jobs[best].priority ||
                    (jobs[i].priority == jobs[best].priority && earlier_job(jobs, i, best)))
                    best = i;
            }
        }
        if (best < 0) {
            int next = -1;
            for (int i = 0; i < n; ++i)
                if (!done[i] && (next < 0 || earlier_job(jobs, i, next))) next = i;
            time = jobs[next].arrival_time;
            continue;
        }
        seg[sc++] = (Segment){best, time, time + jobs[best].cpu_burst};
        time += jobs[best].cpu_burst;
        m[best].completion = time;
        m[best].turnaround = time - jobs[best].arrival_time;
        m[best].waiting = m[best].turnaround - jobs[best].cpu_burst;
        done[best] = 1; ++completed;
    }
    print_results("Non-preemptive Priority Scheduling", jobs, n, m, seg, sc);
    printf("Rule: smaller priority number means higher priority; ties use earlier arrival, then input order.\n");
}

void run_round_robin(const Job jobs[], int n, int quantum) {
    if (!valid_jobs(jobs, n)) return;
    if (quantum <= 0) { printf("Error: Round Robin time quantum must be positive.\n"); return; }

    int remaining[MAX_JOBS], enqueued[MAX_JOBS] = {0};
    int queue[MAX_JOBS * (MAX_SEGMENTS / MAX_JOBS + 1)];
    int qcap = (int)(sizeof(queue) / sizeof(queue[0]));
    int head = 0, tail = 0, qcount = 0, completed = 0, time = 0, sc = 0;
    Metrics m[MAX_JOBS] = {{0}};
    Segment *seg = malloc(sizeof(Segment) * MAX_SEGMENTS);
    if (!seg) { perror("malloc"); return; }
    for (int i = 0; i < n; ++i) remaining[i] = jobs[i].cpu_burst;

    /* Add arrived jobs in arrival/input order. */
    while (completed < n) {
        for (;;) {
            int next = -1;
            for (int i = 0; i < n; ++i)
                if (!enqueued[i] && remaining[i] > 0 && jobs[i].arrival_time <= time &&
                    (next < 0 || earlier_job(jobs, i, next))) next = i;
            if (next < 0) break;
            if (qcount >= qcap) { printf("Error: internal Round Robin queue capacity exceeded.\n"); free(seg); return; }
            queue[tail] = next; tail = (tail + 1) % qcap; ++qcount; enqueued[next] = 1;
        }
        if (qcount == 0) {
            int next = -1;
            for (int i = 0; i < n; ++i)
                if (remaining[i] > 0 && (next < 0 || earlier_job(jobs, i, next))) next = i;
            if (next < 0) break;
            time = jobs[next].arrival_time;
            continue;
        }
        int x = queue[head]; head = (head + 1) % qcap; --qcount;
        int run = remaining[x] < quantum ? remaining[x] : quantum;
        if (sc >= MAX_SEGMENTS) { printf("Error: too many Round Robin time slices.\n"); free(seg); return; }
        seg[sc++] = (Segment){x, time, time + run};
        time += run;
        remaining[x] -= run;
        /* Newly arrived jobs enter before the unfinished job is re-queued. */
        for (;;) {
            int next = -1;
            for (int i = 0; i < n; ++i)
                if (!enqueued[i] && remaining[i] > 0 && jobs[i].arrival_time <= time &&
                    (next < 0 || earlier_job(jobs, i, next))) next = i;
            if (next < 0) break;
            if (qcount >= qcap) { printf("Error: internal Round Robin queue capacity exceeded.\n"); free(seg); return; }
            queue[tail] = next; tail = (tail + 1) % qcap; ++qcount; enqueued[next] = 1;
        }
        if (remaining[x] > 0) {
            if (qcount >= qcap) { printf("Error: internal Round Robin queue capacity exceeded.\n"); free(seg); return; }
            queue[tail] = x; tail = (tail + 1) % qcap; ++qcount;
        } else {
            m[x].completion = time;
            m[x].turnaround = time - jobs[x].arrival_time;
            m[x].waiting = m[x].turnaround - jobs[x].cpu_burst;
            ++completed;
        }
    }
    char title[100]; snprintf(title, sizeof(title), "Round Robin (time quantum = %d)", quantum);
    print_results(title, jobs, n, m, seg, sc);
    free(seg);
}

void compare_schedulers(const Job jobs[], int count, int quantum) {
    printf("\n*** Scheduling comparison: same input job set ***\n");
    run_fcfs(jobs, count);
    run_round_robin(jobs, count, quantum);
    run_priority(jobs, count);
}

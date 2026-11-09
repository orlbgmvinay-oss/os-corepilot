#include "job.h"
#include <stdio.h>
#include <string.h>

int find_job(const Job jobs[], int count, const char *id) {
    if (!jobs || !id || count < 0) return -1;
    for (int i = 0; i < count; ++i)
        if (strcmp(jobs[i].id, id) == 0) return i;
    return -1;
}

int add_job(Job jobs[], int *count, const char *id,
            int arrival_time, int cpu_burst, int priority) {
    if (!jobs || !count || !id || id[0] == '\0' ||
        strlen(id) >= JOB_ID_LEN || *count < 0 || *count >= MAX_JOBS ||
        arrival_time < 0 || cpu_burst <= 0 || priority < 0 ||
        find_job(jobs, *count, id) >= 0) {
        fprintf(stderr, "Error: invalid job data, duplicate ID, or job limit reached.\n");
        return 0;
    }

    Job *j = &jobs[*count];
    snprintf(j->id, sizeof(j->id), "%s", id);
    j->arrival_time = arrival_time;
    j->cpu_burst = cpu_burst;
    j->priority = priority;
    j->status = JOB_WAITING;
    ++(*count);
    return 1;
}

const char *job_status_string(JobStatus status) {
    switch (status) {
        case JOB_WAITING: return "Waiting";
        case JOB_RUNNING: return "Running";
        case JOB_COMPLETED: return "Completed";
        default: return "Unknown";
    }
}

void display_jobs(const Job jobs[], int count) {
    if (!jobs || count <= 0) {
        printf("No jobs have been added.\n");
        return;
    }
    printf("\n%-12s %-10s %-10s %-10s %-12s\n",
           "Job ID", "Arrival", "Burst", "Priority", "Status");
    printf("--------------------------------------------------------------\n");
    for (int i = 0; i < count; ++i)
        printf("%-12s %-10d %-10d %-10d %-12s\n",
               jobs[i].id, jobs[i].arrival_time, jobs[i].cpu_burst,
               jobs[i].priority, job_status_string(jobs[i].status));
}

int update_job_status(Job jobs[], int count, const char *id, JobStatus status) {
    int index = find_job(jobs, count, id);

    if (index < 0 || status < JOB_WAITING || status > JOB_COMPLETED)
        return 0;

    JobStatus current = jobs[index].status;

    /* Allow an unchanged status. */
    if (current == status)
        return 1;

    /* Only allow forward lifecycle transitions. */
    if ((current == JOB_WAITING && status == JOB_RUNNING) ||
        (current == JOB_RUNNING && status == JOB_COMPLETED)) {
        jobs[index].status = status;
        return 1;
    }

    return 0;
}

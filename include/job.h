#ifndef JOB_H
#define JOB_H

#define MAX_JOBS 100
#define JOB_ID_LEN 32

typedef enum {
    JOB_WAITING = 0,
    JOB_RUNNING,
    JOB_COMPLETED
} JobStatus;

typedef struct {
    char id[JOB_ID_LEN];
    int arrival_time;
    int cpu_burst;
    int priority;       /* Smaller number means higher priority. */
    JobStatus status;
} Job;

/* Returns 1 on success, 0 if invalid/full/duplicate. */
int add_job(Job jobs[], int *count, const char *id,
            int arrival_time, int cpu_burst, int priority);
void display_jobs(const Job jobs[], int count);
int update_job_status(Job jobs[], int count, const char *id, JobStatus status);
int find_job(const Job jobs[], int count, const char *id);
const char *job_status_string(JobStatus status);

#endif

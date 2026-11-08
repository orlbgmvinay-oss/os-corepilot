#include "banker.h"
#include "common.h"
#include "ipc.h"
#include "job.h"
#include "logger.h"
#include "resource_limits.h"
#include "scheduler.h"
#include "worker.h"

#include <stdio.h>
#include <string.h>

static int run_scheduling(void)
{
    Job jobs[MAX_JOBS];
    int count = 0;

    if (!add_job(jobs, &count, "J1", 0, 5, 2) ||
        !add_job(jobs, &count, "J2", 1, 3, 1) ||
        !add_job(jobs, &count, "J3", 2, 8, 3) ||
        !add_job(jobs, &count, "J4", 3, 4, 2))
        return 1;

    printf("\n=== Initial Job List ===\n");
    display_jobs(jobs, count);

    compare_schedulers(jobs, count, 2);

    for (int i = 0; i < count; i++)
    {
        if (!update_job_status(jobs, count, jobs[i].id, JOB_COMPLETED))
            return 1;
    }

    printf("\n=== Final Job Status ===\n");
    display_jobs(jobs, count);

    return 0;
}

static int run_ipc_demo(void)
{
    IPCContext sender;
    IPCContext receiver;

    if (ipc_create(&sender) != 0)
        return 1;

    if (ipc_open(&receiver) != 0)
    {
        ipc_cleanup(&sender);
        return 1;
    }

    int result = ipc_send(&sender, "Hello from CorePilot") == 0 &&
                 ipc_receive(&receiver) == 0 ? 0 : 1;
    ipc_close(&receiver);
    ipc_cleanup(&sender);
    return result;
}

static int run_logger_demo(void)
{
    Logger logger;

    if (logger_init(&logger, "logs/campus.log") != 0)
    {
        fprintf(stderr, "Could not initialize the logger.\n");
        return 1;
    }

    if (logger_start(&logger) != 0)
    {
        fprintf(stderr, "Could not start the logger thread.\n");
        logger_destroy(&logger);
        return 1;
    }

    printf("\n=== CorePilot Producer-Consumer Logger ===\n");

    const int producer_count = 3;
    const int messages_per_producer = 5;

    for (int p = 1; p <= producer_count; p++)
    {
        for (int m = 1; m <= messages_per_producer; m++)
        {
            char message[LOGGER_MESSAGE_SIZE];

            snprintf(message, sizeof(message),
                     "Producer %d: Log message %d",
                     p, m);

            if (logger_log(&logger, message) != 0)
            {
                fprintf(stderr, "Failed to enqueue message.\n");
                logger_destroy(&logger);
                return 1;
            }

            printf("Produced: %s\n", message);
        }
    }

    logger_stop(&logger);

    printf("\nProducer wait count: %d\n", logger.producer_waits);
    printf("Consumer wait count: %d\n", logger.consumer_waits);

    logger_destroy(&logger);

    printf("\nLogger stopped successfully.\n");
    printf("Log file: logs/campus.log\n");

    return 0;
}

static int run_worker_demo(void)
{
    pid_t normal_pid = worker_create(WORKER_NORMAL);
    if (normal_pid < 0)
        return 1;

    pid_t failure_pid = worker_create(WORKER_FAILURE);
    if (failure_pid < 0)
    {
        worker_wait(normal_pid);
        return 1;
    }

    int normal_status = worker_wait(normal_pid);
    int failure_status = worker_wait(failure_pid);
    return normal_status == 0 && failure_status == 1 ? 0 : 1;
}

static void print_usage(const char *program)
{
    corepilot_print_banner("CorePilot");
    printf("Usage: %s <feature>\n", program);
    printf("Features: scheduling, banker, ipc, logger, worker, resource-limits\n");
}


int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        print_usage(argv[0]);
        return argc == 1 ? 0 : 2;
    }

    if (strcmp(argv[1], "scheduling") == 0)
        return run_scheduling();
    if (strcmp(argv[1], "banker") == 0)
    {
        banker_demo();
        return 0;
    }
    if (strcmp(argv[1], "ipc") == 0)
        return run_ipc_demo();
    if (strcmp(argv[1], "logger") == 0)
        return run_logger_demo();
    if (strcmp(argv[1], "worker") == 0)
        return run_worker_demo();
    if (strcmp(argv[1], "resource-limits") == 0)
        return resource_limit_demo() == 0 ? 0 : 1;

    print_usage(argv[0]);
    return 2;
}

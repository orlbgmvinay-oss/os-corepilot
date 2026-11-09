#include "banker.h"
#include "common.h"
#include "ipc.h"
#include "job.h"
#include "logger.h"
#include "resource_limits.h"
#include "scheduler.h"
#include "worker.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int run_scheduling(void)
{
    Job jobs[MAX_JOBS];
    int count = 0;

    if (!add_job(jobs, &count, "J1", 0, 5, 2) ||
        !add_job(jobs, &count, "J2", 1, 3, 1) ||
        !add_job(jobs, &count, "J3", 2, 8, 3) ||
        !add_job(jobs, &count, "J4", 3, 4, 2))
        return 1;

    display_jobs(jobs, count);
    compare_schedulers(jobs, count, 2);
    return 0;
}

static int run_ipc_demo(void)
{
    IPCContext sender;

    // Parent creates shared memory and semaphore.
    if (ipc_create(&sender) != 0)
    {
        fprintf(stderr, "Parent: IPC creation failed.\n");
        return 1;
    }

    fflush(stdout);
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        ipc_cleanup(&sender);
        return 1;
    }

    if (pid == 0)
    {
        // Child process.
        IPCContext receiver;

        printf("\n[Child] Process started. PID: %ld\n",
               (long)getpid());

        if (ipc_open(&receiver) != 0)
        {
            fprintf(stderr, "[Child] IPC open failed.\n");
            _exit(1);
        }

        int result = ipc_receive(&receiver);
        ipc_close(&receiver);

        printf("[Child] Process finished.\n");
        fflush(stdout);
        _exit(result == 0 ? 0 : 1);
    }

    // Parent process.
    printf("\n[Parent] Child created. PID: %ld\n", (long)pid);

    int send_result = ipc_send(&sender, "Hello from CorePilot");

    if (send_result != 0)
    {
        fprintf(stderr, "[Parent] Message sending failed.\n");
        kill(pid, SIGTERM);
    }

    int status;
    pid_t waited;

    do
    {
        waited = waitpid(pid, &status, 0);
    }
    while (waited == -1 && errno == EINTR);

    if (waited == -1)
    {
        perror("waitpid failed");
        ipc_cleanup(&sender);
        return 1;
    }

    ipc_cleanup(&sender);

    if (send_result != 0)
        return 1;

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        fprintf(stderr, "[Parent] Child process failed.\n");
        return 1;
    }

    printf("[Parent] Child completed successfully.\n");
    printf("[Parent] IPC resources cleaned up.\n");

    return 0;
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

    int result = logger_log(&logger, "CorePilot logger demonstration");
    logger_destroy(&logger);
    if (result != 0)
    {
        fprintf(stderr, "Could not enqueue the logger demonstration event.\n");
        return 1;
    }
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

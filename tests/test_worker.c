#include "worker.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    // Display output immediately
    setbuf(stdout, NULL);

    printf("====================================\n");
    printf("     WORKER PROCESS TESTING\n");
    printf("====================================\n");

    // Create normal worker
    printf("\nCreating normal worker...\n");

    pid_t normal_pid = worker_create(WORKER_NORMAL);

    if (normal_pid == -1)
    {
        printf("Failed to create normal worker!\n");
        return 1;
    }

    // Create failure worker
    printf("\nCreating failure worker...\n");

    pid_t failure_pid = worker_create(WORKER_FAILURE);

    if (failure_pid == -1)
    {
        printf("Failed to create failure worker!\n");

        // Collect the normal worker's status
        worker_wait(normal_pid);

        return 1;
    }

    // Parent waits for normal worker
    printf("\nParent waiting for workers...\n");

    int normal_status = worker_wait(normal_pid);

    // Parent waits for failure worker
    int failure_status = worker_wait(failure_pid);

    // Display final results
    printf("\n====================================\n");
    printf("          TEST SUMMARY\n");
    printf("====================================\n");

    if (normal_status == 0)
    {
        printf("Normal worker: PASSED\n");
    }
    else
    {
        printf("Normal worker: FAILED\n");
    }

    if (failure_status == EXIT_FAILURE)
    {
        printf("Failure worker: DETECTED SUCCESSFULLY\n");
    }
    else
    {
        printf("Failure worker: DETECTION FAILED\n");
    }

    printf("====================================\n");

    return (normal_status == 0 &&
            failure_status == EXIT_FAILURE) ? 0 : 1;
}

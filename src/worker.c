#include "worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// ==========================================
// 1. Execute the worker process
// ==========================================

void worker_run(int mode)
{
    printf("Worker process started! PID: %d\n", getpid());

    // Normal worker execution
    if (mode == WORKER_NORMAL)
    {
        printf("Worker completed its task successfully!\n");
        exit(EXIT_SUCCESS);
    }

    // Simulated worker failure
    else if (mode == WORKER_FAILURE)
    {
        printf("Worker encountered a simulated failure!\n");
        exit(EXIT_FAILURE);
    }

    // Invalid worker mode
    else
    {
        printf("Invalid worker mode!\n");
        exit(2);
    }
}


// ==========================================
// 2. Create child process using fork()
// ==========================================

pid_t worker_create(int mode)
{
    // Flush output before creating child
    fflush(NULL);

    pid_t pid = fork();

    // Fork failure
    if (pid < 0)
    {
        perror("fork failed");
        return -1;
    }

    // Child process
    if (pid == 0)
    {
        worker_run(mode);
    }

    // Parent process returns child PID
    return pid;
}


// ==========================================
// 3. Wait for child using waitpid()
// ==========================================

int worker_wait(pid_t pid)
{
    int status;

    // Validate process ID
    if (pid <= 0)
    {
        return -1;
    }

    // Wait for the specific child process
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid failed");
        return -1;
    }

    // Check whether child exited normally
    if (WIFEXITED(status))
    {
        int exit_code = WEXITSTATUS(status);

        // Successful execution
        if (exit_code == EXIT_SUCCESS)
        {
            printf("Parent: Worker %d completed successfully.\n", pid);
            return 0;
        }

        // Failed execution
        printf("Parent: Worker %d failed with exit code %d.\n",
               pid, exit_code);

        return exit_code;
    }

    // Check whether child was terminated by a signal
    if (WIFSIGNALED(status))
    {
        printf("Parent: Worker %d terminated by signal %d.\n",
               pid, WTERMSIG(status));

        return -1;
    }

    return -1;
}

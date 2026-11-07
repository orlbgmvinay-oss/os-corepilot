#include "ipc.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
    IPCContext parent_context;

    // Step 1: Parent creates IPC resources
    if (ipc_create(&parent_context) != 0)
    {
        printf("IPC creation failed!\n");
        return 1;
    }

    // Step 2: Create child process
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        ipc_cleanup(&parent_context);
        return 1;
    }

    // Step 3: Child process
    if (pid == 0)
    {
        IPCContext child_context;

        printf("\nChild process started.\n");

        // Close inherited handles and open IPC resources independently
        ipc_close(&parent_context);

        if (ipc_open(&child_context) != 0)
            _exit(1);

        // Wait for and receive parent's message
        int result = ipc_receive(&child_context);

        ipc_close(&child_context);

        fflush(stdout);
        _exit(result == 0 ? 0 : 1);
    }

    // Step 4: Parent process
    printf("\nParent process started.\n");

    sleep(1);

    if (ipc_send(&parent_context, "Hello Child") != 0)
    {
        printf("Message sending failed!\n");
    }

    // Step 5: Wait for child process
    int status;

    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid failed");
        ipc_cleanup(&parent_context);
        return 1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        printf("\nChild completed successfully!\n");
    }
    else
    {
        printf("\nChild process failed!\n");
    }

    // Step 6: Clean up IPC resources
    ipc_cleanup(&parent_context);

    return 0;
}
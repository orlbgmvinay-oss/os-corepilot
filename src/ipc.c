#include "ipc.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

int ipc_create(IPCContext *context)
{
    if (context == NULL)
        return -1;

    context->shm_fd = -1;
    context->shared_data = NULL;
    context->semaphore = SEM_FAILED;
    context->creator = false;

    // Step 1: Create shared memory
    context->shm_fd = shm_open(
        SHM_NAME,
        O_CREAT | O_EXCL | O_RDWR,
        0666
    );

    if (context->shm_fd == -1)
    {
        perror("shm_open failed");
        return -1;
    }

    // Step 2: Set shared memory size
    if (ftruncate(context->shm_fd, sizeof(IPCSharedData)) == -1)
    {
        perror("ftruncate failed");
        goto cleanup;
    }

    // Step 3: Map shared memory
    context->shared_data = mmap(
        NULL,
        sizeof(IPCSharedData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        context->shm_fd,
        0
    );

    if (context->shared_data == MAP_FAILED)
    {
        perror("mmap failed");
        context->shared_data = NULL;
        goto cleanup;
    }

    // Step 4: Create named semaphore
    context->semaphore = sem_open(
        SEM_NAME,
        O_CREAT | O_EXCL,
        0666,
        0
    );

    if (context->semaphore == SEM_FAILED)
    {
        perror("sem_open failed");
        goto cleanup;
    }

    context->creator = true;

    printf("Shared memory and semaphore created successfully!\n");

    return 0;

cleanup:

    if (context->shared_data != NULL)
    {
        munmap(context->shared_data, sizeof(IPCSharedData));
        context->shared_data = NULL;
    }

    close(context->shm_fd);
    context->shm_fd = -1;

    shm_unlink(SHM_NAME);

    return -1;
}


int ipc_open(IPCContext *context)
{
    if (context == NULL)
        return -1;

    context->shm_fd = -1;
    context->shared_data = NULL;
    context->semaphore = SEM_FAILED;
    context->creator = false;

    // Step 1: Open existing shared memory
    context->shm_fd = shm_open(
        SHM_NAME,
        O_RDWR,
        0666
    );

    if (context->shm_fd == -1)
    {
        perror("shm_open failed");
        return -1;
    }

    // Step 2: Map shared memory
    context->shared_data = mmap(
        NULL,
        sizeof(IPCSharedData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        context->shm_fd,
        0
    );

    if (context->shared_data == MAP_FAILED)
    {
        perror("mmap failed");
        context->shared_data = NULL;
        goto cleanup;
    }

    // Step 3: Open existing named semaphore
    context->semaphore = sem_open(
        SEM_NAME,
        0
    );

    if (context->semaphore == SEM_FAILED)
    {
        perror("sem_open failed");

        munmap(context->shared_data, sizeof(IPCSharedData));
        context->shared_data = NULL;

        goto cleanup;
    }

    printf("Shared memory and semaphore opened successfully!\n");

    return 0;

cleanup:

    close(context->shm_fd);
    context->shm_fd = -1;

    return -1;
}

int ipc_send(IPCContext *context, const char *message)
{
    if (context == NULL || message == NULL ||
        context->shared_data == NULL ||
        context->semaphore == SEM_FAILED)
    {
        return -1;
    }

    // Step 1: Write message into shared memory
    snprintf(
        context->shared_data->message,
        IPC_MESSAGE_SIZE,
        "%s",
        message
    );

    // Step 2: Notify the child that data is ready
    if (sem_post(context->semaphore) == -1)
    {
        perror("sem_post failed");
        return -1;
    }

    printf("Message sent successfully!\n");

    return 0;
}

int ipc_receive(IPCContext *context)
{
    if (context == NULL ||
        context->shared_data == NULL ||
        context->semaphore == SEM_FAILED)
    {
        return -1;
    }

    // Step 1: Wait until the parent sends a signal
    while (sem_wait(context->semaphore) == -1)
    {
        if (errno == EINTR)
            continue;

        perror("sem_wait failed");
        return -1;
    }

    // Step 2: Read the message from shared memory
    printf("Child received message: %s\n",
           context->shared_data->message);

    return 0;
}

void ipc_close(IPCContext *context)
{
    if (context == NULL)
        return;

    // Step 1: Unmap shared memory
    if (context->shared_data != NULL)
    {
        munmap(context->shared_data, sizeof(IPCSharedData));
        context->shared_data = NULL;
    }

    // Step 2: Close shared memory file descriptor
    if (context->shm_fd != -1)
    {
        close(context->shm_fd);
        context->shm_fd = -1;
    }

    // Step 3: Close named semaphore
    if (context->semaphore != SEM_FAILED)
    {
        sem_close(context->semaphore);
        context->semaphore = SEM_FAILED;
    }
}


void ipc_cleanup(IPCContext *context)
{
    if (context == NULL)
        return;

    // Step 1: Close resources
    ipc_close(context);

    // Step 2: Remove shared memory and semaphore
    if (context->creator)
    {
        if (shm_unlink(SHM_NAME) == -1)
            perror("shm_unlink failed");

        if (sem_unlink(SEM_NAME) == -1)
            perror("sem_unlink failed");

        printf("IPC resources cleaned up successfully!\n");

        context->creator = false;
    }
}

#ifndef IPC_H
#define IPC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <stdbool.h>

#define SHM_NAME "/campus_compute_shm"
#define SEM_NAME "/campus_compute_sem"

#define IPC_MESSAGE_SIZE 256

typedef struct
{
    char message[IPC_MESSAGE_SIZE];

} IPCSharedData;

typedef struct
{
    int shm_fd;

    IPCSharedData *shared_data;

    sem_t *semaphore;

    bool creator;

} IPCContext;


// IPC function declarations

int ipc_create(IPCContext *context);

int ipc_open(IPCContext *context);

int ipc_send(IPCContext *context, const char *message);

int ipc_receive(IPCContext *context);

void ipc_close(IPCContext *context);

void ipc_cleanup(IPCContext *context);

#endif
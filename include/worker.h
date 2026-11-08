#ifndef WORKER_H
#define WORKER_H

#include <sys/types.h>

// Worker execution modes
#define WORKER_NORMAL 0
#define WORKER_FAILURE 1

// Function declarations
pid_t worker_create(int mode);

int worker_wait(pid_t pid);

void worker_run(int mode);

#endif
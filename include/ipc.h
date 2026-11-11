/* ipc.h - FR4: POSIX shared memory + named semaphores between two processes. */
#ifndef CAMPUS_IPC_H
#define CAMPUS_IPC_H

#include "scheduler.h"

/* Runs the controller/reporter exchange. Returns 1 on success, 0 on failure.
 * All IPC objects created are removed before returning, even on errors. */
int ipc_demo(const char *jobfile, Policy policy, int quantum, int verbose);

/* Removes ONLY the objects this program creates (SHM_NAME, SEM_REQ_NAME,
 * SEM_ACK_NAME). Returns how many existed and were removed. */
int ipc_unlink_all(int verbose);

/* Reports (and returns) how many of the program's IPC objects still exist. */
int ipc_objects_present(int verbose);

int run_ipc_cmd(int argc, char **argv);

#endif /* CAMPUS_IPC_H */

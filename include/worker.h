/* worker.h - FR6: worker processes, failure detection and a watchdog. */
#ifndef CAMPUS_WORKER_H
#define CAMPUS_WORKER_H

/* Spawns workers for `mode` ("normal", "fail", "crash", "hang" or "mix"),
 * supervises them with fork()/waitpid() and prints a report. Returns 1 when
 * every observed outcome matches what was injected, 0 otherwise. */
int worker_scenario(const char *mode, int timeout_seconds, int verbose);

int run_worker_cmd(int argc, char **argv);

#endif /* CAMPUS_WORKER_H */

# Project layout

- `include/` contains the public headers for each module.
- `src/` contains the application entry point and module implementations.
- `tests/` contains standalone tests for the six feature areas.
- `logs/` is the output directory used by the logger demonstration and test.

## Modules

- `banker.c` implements Banker's safety algorithm.
- `common.c` provides shared command-line presentation helpers.
- `job.c` and `scheduler.c` provide job management and FCFS, Round Robin, and non-preemptive priority scheduling.
- `ipc.c` implements shared-memory and semaphore-based IPC.
- `logger.c` provides a producer-consumer logging queue.
- `worker.c` creates and waits for worker processes.
- `resource_limits.c` demonstrates process file-descriptor limits.
- `main.c` exposes each module demonstration through the `campus_compute` command.

Build the application with `make` and run the complete test suite with `make test`.
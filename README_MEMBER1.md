# CampusCompute – Member 1 modules (FR1, FR2, FR5)

This folder contains a working implementation of:
- FR1: Job management (`job.c`, `job.h`)
- FR2: FCFS, Round Robin, and non-preemptive Priority scheduling (`scheduler.c`, `scheduler.h`)
- FR5: Banker's safety algorithm (`banker.c`, `banker.h`)

## Build and run on Linux / Ubuntu / WSL

```bash
make
./member1_demo
```

Clean build artifacts:

```bash
make clean
```

## Scheduling conventions

- Arrival time must be >= 0; CPU burst must be > 0; priority must be >= 0.
- FCFS ties: earlier arrival, then original input order.
- Round Robin: FIFO ready queue; newly arrived jobs are enqueued before the just-preempted job is re-queued.
- Priority scheduling is non-preemptive. Smaller priority number means higher priority. Ties use earlier arrival, then original input order.
- Waiting time = turnaround time - CPU burst.
- Turnaround time = completion time - arrival time.

## Integration into the team's application

Copy `job.c/.h`, `scheduler.c/.h`, and `banker.c/.h` into the main project. Compile them with the team's `main.c` and other modules. Include the relevant headers in `main.c`.

Useful functions:

```c
add_job(jobs, &count, "J1", 0, 5, 2);
display_jobs(jobs, count);
run_fcfs(jobs, count);
run_round_robin(jobs, count, 2);
run_priority(jobs, count);
compare_schedulers(jobs, count, 2);
run_banker_interactive();
banker_demo();
```

`demo_main.c` is only a standalone test driver; do not include it if the team already has its own `main.c`.

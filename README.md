# CampusCompute — Intelligent Job Scheduling and Resource Management

Command-line prototype (C, Linux/Ubuntu/WSL) for the OS Level 3 mini-project.
Every result is computed by the code at run time from input files in `tests/`.

## Prerequisites
Ubuntu or WSL Ubuntu with `gcc` and `make` (`sudo apt install build-essential`).

## Build
```
make            # builds ./campuscompute  (flags: -Wall -Wextra, no warnings)
make clean      # removes build/ and the binary, plus this program's own IPC objects
```
Manual build (no make): `gcc -std=gnu11 -Wall -Wextra -O2 -pthread -Iinclude src/*.c -o campuscompute -pthread -lrt -lm`

## Commands
| Command | Feature |
|---|---|
| `./campuscompute help` | usage (FR8) |
| `./campuscompute sched fcfs\|rr\|priority <jobfile> [--quantum N] [--aging N]` | FR1, FR2 |
| `./campuscompute sched compare <jobfile> [--quantum N] [--aging N]` | same job set, all policies |
| `./campuscompute logger [--producers P] [--consumers C] [--items N] [--capacity K]` | FR3 |
| `./campuscompute ipc [jobfile] [--policy fcfs\|rr\|priority] [--quantum N]` | FR4 |
| `./campuscompute banker <statefile>` | FR5 |
| `./campuscompute worker normal\|fail\|crash\|hang\|mix [--timeout S]` | FR6 |
| `./campuscompute rlimit nofile\|mem\|cpu\|all` | FR7 |
| `./campuscompute demo [jobfile]` | integrated run + summary |
| `./campuscompute cleanup [--logs]` | remove this program's IPC objects (and optionally its log) |
| `./campuscompute gen <count> <seed> <outfile>` | reproducible random job file |

Invalid commands, options, or malformed input print an error and usage, and exit with status 2.

## Rules stated by the program
* **FCFS:** equal arrival -> lower job ID first.
* **Round Robin:** default quantum 3 (`--quantum`). A job arriving exactly when a slice ends joins the queue *before* the pre-empted job.
* **Priority (non-preemptive):** lower number = more urgent; ties -> earlier arrival, then lower job ID.
  Optional `--aging N` improves a waiting job's effective priority by 1 per N time units (starvation guard).
* **Banker's:** Need = Max - Allocation; scan P0..Pn-1 and pick the first unfinished process with Need <= Work.

## Input formats
* Jobs (`tests/jobs_*.txt`): `id name arrival burst priority`, `#` comments.
* Banker (`tests/banker_*.txt`): keywords `processes`, `resources`, `names`, `available`, `max`, `allocation`, optional `request <pid> v1..vM`.

## Beyond the brief
Priority aging; Banker's resource-request check (grant/deny with rollback); per-policy CPU utilisation,
throughput and context-switch counts; worker watchdog that kills a simulated hang; three rlimit
experiments (open files, address space, CPU time); reproducible job generator; one shared log for all modules.

## Clean-up and safety
Only these POSIX objects are ever created or removed: `/campuscompute_shm`, `/campuscompute_req`,
`/campuscompute_ack` (visible in `/dev/shm`), plus `logs/campuscompute.log`. All simulated failures are labelled
`SIMULATED`. Check manually: `ls /dev/shm | grep campuscompute` (should print nothing) and `ps -ef | grep campuscompute`.

## Tests
`make test` (or `bash tests/run_tests.sh`) runs every command, including invalid ones, and checks exit codes and key output.

## Screenshot guide (S01-S24)
Set the prompt first: `export SRN='YOUR_FULL_SRN'; export PS1='[$SRN] \w\$ '`

| # | Command |
|---|---|
| S01 | `pwd; uname -a; cat /etc/os-release \| head -3` |
| S02 | `gcc --version; make --version` |
| S03 | `ls -R include src tests Makefile README.md` |
| S04 | `make clean && make` |
| S05 | `./campuscompute help` |
| S06 | `./campuscompute sched fcfs tests/jobs_basic.txt` |
| S07 | `./campuscompute sched rr tests/jobs_basic.txt --quantum 3` |
| S08 | `./campuscompute sched priority tests/jobs_basic.txt` (and `--aging 2`) |
| S09 | `./campuscompute sched compare tests/jobs_basic.txt --aging 2` |
| S10 | `./campuscompute logger` |
| S11 | `./campuscompute logger --producers 4 --consumers 1 --capacity 1 --items 6` |
| S12 | `./campuscompute ipc` (top part) |
| S13 | `./campuscompute ipc` (cleanup part); then `ls /dev/shm` |
| S14 | `./campuscompute banker tests/banker_safe.txt` |
| S15 | `./campuscompute banker tests/banker_unsafe.txt` |
| S16 | `./campuscompute worker normal` |
| S17 | `./campuscompute worker fail` (also `worker mix`) |
| S18 | `./campuscompute rlimit` |
| S19 | `./campuscompute bogus` and `./campuscompute sched fcfs tests/jobs_bad_format.txt` |
| S20 | `./campuscompute cleanup; ls /dev/shm; ps -ef \| grep campuscompute` |
| S21 | `./campuscompute demo` (summary at the end) |
| S22 | `ls -l logs; head -20 logs/campuscompute.log` |
| S23 | `ls -l campuscompute Makefile src include` |
| S24 | `zip -r ... ; unzip -l ... ; unzip -t ...` |

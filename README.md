# CampusCompute
### Intelligent Job Scheduling and Resource Management

CampusCompute is a command-line Operating Systems mini-project developed using **C on Linux/Ubuntu (WSL)**.

It demonstrates fundamental operating-system concepts through CPU scheduling, process management, concurrent logging, inter-process communication, resource-safety analysis, worker-failure detection, and resource-limit management.

---

## 1. Project Overview

In a shared computing environment, multiple processes compete for CPU time and system resources.

CampusCompute demonstrates how an operating system can manage these processes through scheduling algorithms, synchronization mechanisms, inter-process communication, and resource-management techniques.

## 2. Technologies Used

| Technology | Purpose |
|---|---|
| C | Core programming language |
| Linux / Ubuntu / WSL | Execution environment |
| GCC | C compiler |
| GNU Make | Build automation |
| POSIX Threads | Concurrent logging |
| Mutexes and Condition Variables | Thread synchronization |
| `fork()` and `waitpid()` | Process management |
| POSIX Shared Memory | Inter-process communication |
| POSIX Named Semaphores | Process synchronization |
| Banker's Algorithm | Resource-safety analysis |
| `getrlimit()` and `setrlimit()` | Resource-limit management |
| Git and GitHub | Version control and collaboration |

## 3. Functional Requirements

| ID | Module | Implementation |
|---|---|---|
| FR1 | Job Management | Job creation, validation, and lifecycle status |
| FR2 | CPU Scheduling | FCFS, Round Robin, and Non-preemptive Priority |
| FR3 | Concurrent Logger | Bounded producer-consumer queue using pthreads |
| FR4 | Inter-Process Communication | POSIX shared memory and named semaphore |
| FR5 | Resource Safety | Banker's safety algorithm |
| FR6 | Worker Management | Child-process creation and failure detection |
| FR7 | Resource Limits | File descriptor limit demonstration and cleanup |
| FR8 | Command-Line Interface | Feature commands and invalid-command handling |

All seven test modules have passed, and the integrated application has been built and tested.

## 4. Project Structure

```text
CampusCompute/
│
├── include/
│   ├── common.h
│   ├── job.h
│   ├── scheduler.h
│   ├── banker.h
│   ├── logger.h
│   ├── ipc.h
│   ├── worker.h
│   └── resource_limits.h
│
├── src/
│   ├── main.c
│   ├── common.c
│   ├── job.c
│   ├── scheduler.c
│   ├── banker.c
│   ├── logger.c
│   ├── ipc.c
│   ├── worker.c
│   └── resource_limits.c
│
├── tests/
│   ├── test_job.c
│   ├── test_scheduler.c
│   ├── test_banker.c
│   ├── test_logger.c
│   ├── test_ipc.c
│   ├── test_worker.c
│   └── test_resource_limits.c
│
├── docs/
│   └── architecture.md
│
├── logs/
│   └── .gitkeep
│
├── Makefile
├── README.md
├── LICENSE
└── .gitignore
```

## 5. Prerequisites

Required environment:

- Ubuntu / Linux / WSL Ubuntu
- GCC compiler
- GNU Make
- POSIX-compatible environment

Install development tools:

```bash
sudo apt update
sudo apt install build-essential gcc make
```

Verify installation:

```bash
gcc --version
make --version
```

## 6. Compilation

Clone the repository:

```bash
git clone https://github.com/orlbgmvinay-oss/os-corepilot.git
```

Navigate into the project:

```bash
cd os-corepilot
```

Compile the application:

```bash
make
```

## 7. Running the Application

General command:

```bash
./campus_compute <feature>
```

Available features:

| Feature | Command |
|---|---|
| CPU Scheduling | `./campus_compute scheduling` |
| Banker's Algorithm | `./campus_compute banker` |
| Concurrent Logger | `./campus_compute logger` |
| Inter-Process Communication | `./campus_compute ipc` |
| Worker Management | `./campus_compute worker` |
| Resource Limits | `./campus_compute resource-limits` |

Display available commands:

```bash
./campus_compute
```

Test invalid command handling:

```bash
./campus_compute invalid
```

## 8. Running Tests

Execute all seven test modules:

```bash
make test
```

The test suite covers:

1. Job management and validation.
2. Banker's safety algorithm.
3. Shared-memory IPC.
4. Concurrent producer-consumer logger.
5. Resource-limit management.
6. CPU scheduling algorithms.
7. Worker-process management.

## 9. Generated Logs

The concurrent logger generates event entries in:

```text
logs/campus.log
```

Display the log:

```bash
cat logs/campus.log
```

Count the generated entries:

```bash
wc -l logs/campus.log
```

## 10. Cleanup

Remove generated object files, executables, test binaries, and generated application logs:

```bash
make clean
```

Rebuild the application:

```bash
make
```

## 11. Team Contributions

This project is developed collaboratively by a two-member team.

| Member | Responsibilities |
|---|---|
| Member 1 | Job Management, CPU Scheduling, Banker's Algorithm |
| Member 2 | Concurrent Logger, IPC, Worker Failure Detection, Resource Limits |
| Both Members | Integration, CLI, testing, documentation, and debugging |

## 12. Version Control

Git and GitHub are used for collaborative development.

Development work is organized using feature branches and integrated into the main branch through Pull Requests.

## 13. Project Objective

The objective of CampusCompute is to demonstrate practical implementation of operating-system concepts through modular C programs, with emphasis on process scheduling, synchronization, inter-process communication, and resource management.

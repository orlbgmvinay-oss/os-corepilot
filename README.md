# CampusCompute
### Intelligent Job Scheduling and Resource Management

CampusCompute is a command-line Operating Systems mini-project developed in **C on Linux/Ubuntu (WSL)**. It demonstrates important OS concepts through job scheduling, concurrent logging, inter-process communication, resource-safety analysis, worker-process management, and resource limits.

---

## Project Overview

In a shared computing environment, multiple processes compete for CPU time and system resources.

CampusCompute demonstrates how an operating system can manage these processes efficiently using scheduling algorithms, synchronization mechanisms, IPC, and resource-management techniques.

## Technologies Used

- **Programming Language:** C
- **Operating System:** Linux / Ubuntu / WSL
- **Compiler:** GCC
- **Build Tool:** GNU Make
- **Thread Management:** POSIX Threads (pthreads)
- **Process Management:** `fork()`, `waitpid()`
- **Inter-Process Communication:** POSIX Shared Memory
- **Synchronization:** POSIX Named Semaphores, Mutexes, Condition Variables
- **Resource Management:** Banker's Algorithm, `getrlimit()`, `setrlimit()`
- **Version Control:** Git and GitHub

## Functional Requirements

| ID | Module | Description | Status |
|---|---|---|---|
| FR1 | Job Management | Job creation, validation, and lifecycle tracking | Implementation present; verification pending |
| FR2 | CPU Scheduling | FCFS, Round Robin, and Priority Scheduling | Implementation present; verification pending |
| FR3 | Concurrent Logger | Bounded producer-consumer queue using pthreads | Implemented; tested |
| FR4 | Inter-Process Communication | Shared memory and semaphore synchronization | Implemented; tested |
| FR5 | Resource Safety | Banker's safety algorithm | Implementation present; verification pending |
| FR6 | Worker Management | Process creation and failure detection | Implemented; tested |
| FR7 | Resource Limits | Process resource-limit demonstration and cleanup | Implemented; tested |
| FR8 | Command-Line Interface | Commands, input validation, and error handling | Integration pending |

*Status will be updated as modules are integrated and verified.*

## Project Structure

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
│   ├── test_scheduler.c
│   ├── test_banker.c
│   ├── test_logger.c
│   ├── test_ipc.c
│   ├── test_worker.c
│   └── test_resource_limits.c
│
├── docs/
│   ├── architecture.md
│   └── README_MEMBER1.md
│
├── logs/
├── Makefile
├── .gitignore
└── README.md
```

## Prerequisites

Install the required development tools on Ubuntu:

```bash
sudo apt update
sudo apt install build-essential gcc make
```

Verify the installation:

```bash
gcc --version
make --version
```

## Compilation and Execution

Compile the complete project:

```bash
make
```

Run the application:

```bash
make run
```

**Note:** The complete application depends on integration of all modules. Until integration is complete, use the individual test targets below.

## Running Individual Tests

Run the available module tests using Make:

| Module | Command |
|---|---|
| Logger | `make test_logger` |
| IPC | `make test_ipc` |
| Worker Management | `make test_worker` |
| Resource Limits | `make test_resource_limits` |
| CPU Scheduling | `make test_scheduler` |
| Banker's Algorithm | `make test_banker` |

## Cleanup

Remove generated object files, executables, and test binaries:

```bash
make clean
```

## Team Contribution

This project is developed collaboratively by a two-member team.

| Member | Responsibilities |
|---|---|
| Member 1 | Job Management, CPU Scheduling, Banker's Algorithm |
| Member 2 | Concurrent Logger, IPC, Worker Failure Detection, Resource Limits |
| Both Members | Integration, CLI, testing, documentation, and debugging |

## Version Control Workflow

The project uses Git and GitHub for collaborative development.

Development work is maintained in separate feature branches and integrated into the `main` branch through Pull Requests.

## Project Objective

The objective of CampusCompute is to demonstrate practical implementation of operating-system concepts through modular C programs, with particular emphasis on process scheduling, synchronization, inter-process communication, and resource management.
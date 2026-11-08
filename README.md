# CampusCompute – Intelligent Job Scheduling and Resource Management

**CampusCompute** is a Linux-based operating system mini-project developed in C. It demonstrates CPU scheduling, multithreaded synchronization, inter-process communication (IPC), resource allocation safety, worker process management, and system resource monitoring using POSIX mechanisms.

## Project Overview

The project simulates a university computing environment where multiple jobs compete for CPU time and system resources.

It demonstrates important Operating System concepts through practical implementations of process management, scheduling algorithms, synchronization mechanisms, and resource management.

## Functional Requirements

| ID | Module | Description |
|---|---|---|
| FR1 | Job Management | Job creation, validation and lifecycle management |
| FR2 | CPU Scheduling | FCFS, Round Robin and Priority Scheduling |
| FR3 | Concurrent Logger | Producer-consumer problem using POSIX threads |
| FR4 | Inter-Process Communication | POSIX shared memory and named semaphores |
| FR5 | Banker's Algorithm | Safe and unsafe resource-state detection |
| FR6 | Worker Management | Process creation and failure detection using fork() and waitpid() |
| FR7 | Resource Limits | Resource monitoring and limits using setrlimit() |
| FR8 | Command-Line Interface | Integrated command-line interface and error handling |

## Technologies Used

- Programming Language: C
- Operating System: Linux / Ubuntu / WSL
- Compiler: GCC
- Build Tool: GNU Make
- Multithreading: POSIX Threads (pthread)
- Process Management: fork(), waitpid()
- Inter-Process Communication: POSIX Shared Memory
- Synchronization: POSIX Named Semaphores
- Resource Management: setrlimit()
- Version Control: Git and GitHub

## Project Structure

```text
os-corepilot/
├── include/
│   ├── common.h
│   ├── job.h
│   ├── scheduler.h
│   ├── logger.h
│   ├── ipc.h
│   ├── banker.h
│   ├── worker.h
│   └── resource_limits.h
│
├── src/
│   ├── main.c
│   ├── common.c
│   ├── job.c
│   ├── scheduler.c
│   ├── logger.c
│   ├── ipc.c
│   ├── banker.c
│   ├── worker.c
│   └── resource_limits.c
│
├── tests/
│   ├── test_scheduler.c
│   ├── test_banker.c
│   ├── test_ipc.c
│   ├── test_logger.c
│   ├── test_resource_limits.c
│   └── test_worker.c
│
├── logs/
├── Makefile
├── README.md
└── LICENSE
```

## Prerequisites

Make sure the following tools are installed:

- GCC compiler
- GNU Make
- Linux / Ubuntu environment

Check the installations:

```bash
gcc --version
make --version
```

## Getting Started

Clone the repository:

```bash
git clone https://github.com/orlbgmvinay-oss/os-corepilot.git
```

Navigate into the project:

```bash
cd os-corepilot
```

## Testing Implemented Modules

The following Makefile targets are available for the currently implemented modules:

```bash
make test_logger
make test_ipc
make test_worker
make test_resource_limits
```

## Cleanup

Remove generated object files and test executables:

```bash
make clean
```

## Team Contributions

The project is developed collaboratively by two members.

| Member | Responsibilities |
|---|---|
| Member 1 | Job Management, CPU Scheduling and Banker's Algorithm |
| Member 2 | Concurrent Logger, IPC, Worker Management and Resource Limits |

Both members collaborate on application integration, testing, documentation and the command-line interface.

## Development Status

The project is under development.

- FR3 – Concurrent Logger: Implemented and tested.
- FR4 – Inter-Process Communication: Implemented and tested.
- FR6 – Worker Failure Detection: Implemented and tested.
- FR7 – Resource Limits: Implemented and tested.
- FR1, FR2 and FR5: Pending implementation.
- FR8 – CLI Integration: Pending implementation.

## Repository

GitHub: https://github.com/orlbgmvinay-oss/os-corepilot

## License

See the LICENSE file for details.

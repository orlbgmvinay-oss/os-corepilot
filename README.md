# os-corepilot

CorePilot is a C project demonstrating CPU scheduling, Banker's deadlock avoidance, POSIX inter-process communication, concurrent logging, worker processes, and resource limits.

## Build

```sh
make
```

Run a feature demonstration with one of:

```sh
./campus_compute scheduling
./campus_compute banker
./campus_compute ipc
./campus_compute logger
./campus_compute worker
./campus_compute resource-limits
```

## Tests

Build and run all six feature tests:

```sh
make test
```

Remove generated binaries, object files, and logs:

```sh
make clean
```

See [docs/architecture.md](docs/architecture.md) for the source layout and module overview.

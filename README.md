# POSIX and MPI Concurrency in C

This repository contains various C programs demonstrating concurrent and parallel programming techniques using POSIX standards and MPI (Message Passing Interface).

## Contents

- **lock_free_queue.c**: Implementation and performance comparison of a lock-free queue using atomic operations and a POSIX mutex-based queue.
- **mcount_fork.c**: Program to count the total number of bytes read from multiple file descriptors concurrently.
- **MPI_coin_toss_protocol.c**: MPI application for electing a ruler using a multi-round coin toss protocol.
- **pipe.c**: Function to pipe the output of one command to the input of another, handling input/output redirection and retrieving exit statuses.

## Build and Run

### Prerequisites

- GCC
- MPICH or OpenMPI

### Compilation

To compile all programs, run:

```bash
make
```

### Cleaning up

To clean up the compiled executables and object files, run:

```bash
make clean
```

### Run

```bash
./lock_free_queue
./mcount_fork
./pipe

mpirun -np <number_of_processes> ./MPI_coin_toss_protocol
```


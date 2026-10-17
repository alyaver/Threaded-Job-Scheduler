# poolsched

A multithreaded memory-pool scheduler written in C (pthreads). A set of jobs, each requesting a block of memory, is placed on a shared queue. Worker threads pull jobs, allocate from a fixed-size memory pool using a **first-fit** or **next-fit** policy, then free the block, merging adjacent free regions (coalescing). Output lines (`SUBMIT`, `FREE`, `COALESCE`, `DONE`) show what happened.

## Layout

```
include/   poolsched.h          shared declarations
src/       main.c pool.c worker.c jobqueue.c
inputs/    jobs-*.txt           sample job files
Makefile
```

## Build

```
make          # builds ./poolsched
make clean    # removes objects and binary
```

Requires `gcc` and pthreads.

## Run

```
./poolsched <num_workers> <job_file> -p first|next
```

- `num_workers`: 1-8 worker threads
- `job_file`: file with one job per line, `<bytes_needed> <duration>`
- `-p`: allocation policy (`first` = first-fit, `next` = next-fit)

Examples:

```
./poolsched 2 inputs/jobs-basic.txt -p first
./poolsched 4 inputs/jobs-stress.txt -p next
```

Sample inputs: `jobs-basic.txt`, `jobs-coalesce.txt`, `jobs-fragmentation.txt`, `jobs-stress.txt`.

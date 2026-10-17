#ifndef POOLSCHED_H
#define POOLSCHED_H

#include <stddef.h>
#include <pthread.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */

#define POOL_SIZE (1 << 20)  /* 1 MiB */

/* ------------------------------------------------------------------ */
/*  Block header — embedded at the start of every block in the pool   */
/*                                                                    */
/*  Each block in the pool begins with one of these headers.  The     */
/*  "payload" (usable memory returned to the caller) starts           */
/*  immediately after the header.                                     */
/*                                                                    */
/*      [ header | ........... payload ........... ]                   */
/*                ^                                                   */
/*                pointer returned by pool_alloc                      */
/*                                                                    */
/*  Blocks are kept in a singly-linked list ordered by address.       */
/*  This makes coalescing straightforward: adjacent free blocks are   */
/*  always neighbors in the list.                                     */
/* ------------------------------------------------------------------ */

typedef struct block_header {
    size_t size;                /* usable payload bytes (excludes header) */
    int    free;                /* 1 = free, 0 = allocated */
    struct block_header *next;  /* next block in address order */
} block_header_t;

#define HEADER_SIZE   sizeof(block_header_t)
#define MIN_BLOCK_SIZE (HEADER_SIZE + 1)

/* ------------------------------------------------------------------ */
/*  Allocation policy                                                 */
/* ------------------------------------------------------------------ */

typedef enum { POLICY_FIRST_FIT, POLICY_NEXT_FIT } alloc_policy_t;

/* ------------------------------------------------------------------ */
/*  Pool allocator API (pool.c)                                       */
/*                                                                    */
/*  You will implement these functions.  See pool.c for details.      */
/* ------------------------------------------------------------------ */

void  pool_init(alloc_policy_t policy);
void *pool_alloc(size_t size, int job_id);
void  pool_free(void *ptr, int job_id);
void  pool_destroy(void);

/* Offset of a payload pointer from the start of the pool. */
size_t pool_offset(void *ptr);

/* Debug: print free-list state to stderr. */
void print_pool(void);

/* ------------------------------------------------------------------ */
/*  Job definition                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    int    id;
    size_t need;      /* bytes to allocate */
    int    work_ms;   /* simulated work duration */
} job_t;

/* ------------------------------------------------------------------ */
/*  Job queue API (jobqueue.c) — PROVIDED, do not modify              */
/*                                                                    */
/*  A thread-safe bounded queue.  Workers call jq_pop() to get the    */
/*  next job; it blocks until a job is available or shutdown is        */
/*  signaled.  The main thread calls jq_push() and jq_shutdown().     */
/* ------------------------------------------------------------------ */

#define JOB_QUEUE_CAP 256

typedef struct {
    job_t            buf[JOB_QUEUE_CAP];
    int              head, tail, count;
    int              shutdown;
    pthread_mutex_t  lock;
    pthread_cond_t   cond;
} job_queue_t;

void  jq_init(job_queue_t *q);
void  jq_push(job_queue_t *q, job_t job);
int   jq_pop(job_queue_t *q, job_t *out);  /* returns 0 on shutdown */
void  jq_shutdown(job_queue_t *q);
void  jq_destroy(job_queue_t *q);

/* ------------------------------------------------------------------ */
/*  Worker API (worker.c)                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    job_queue_t *queue;
    int          worker_id;
} worker_arg_t;

void *worker_main(void *arg);

/* ------------------------------------------------------------------ */
/*  Shared output lock (avoids interleaved lines on stdout)           */
/*                                                                    */
/*  Lock this mutex around every fprintf(stdout, ...) call so that    */
/*  output lines from different threads never interleave.             */
/* ------------------------------------------------------------------ */

extern pthread_mutex_t output_lock;

#endif /* POOLSCHED_H */

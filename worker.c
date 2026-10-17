#include "poolsched.h"
#include <stdio.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/*  sleep_ms — provided, do not modify                                */
/* ------------------------------------------------------------------ */

static void sleep_ms(int ms)
{
    struct timespec ts = { .tv_sec = ms / 1000,
                           .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------ */
/*  worker_main — entry point for each worker thread                  */
/*                                                                    */
/*  Each worker loops:                                                */
/*    1. Dequeue a job with jq_pop().  If it returns 0, the queue     */
/*       has shut down — break out of the loop and return.            */
/*    2. Allocate memory for the job using pool_alloc().              */
/*    3. Print a WORK line (protect with output_lock).                */
/*    4. Simulate work by sleeping for job.work_ms milliseconds.      */
/*    5. Free the memory using pool_free().                           */
/* ------------------------------------------------------------------ */

void *worker_main(void *arg)
{

    /* TODO:
     * Loop while jq_pop(q, &job) returns non-zero:
     *   1. Call pool_alloc(job.need, job.id) to get a pointer.
     *   2. Lock output_lock, print WORK line, flush, unlock.
     *      Format: "WORK job=%d ms=%d\n"
     *   3. Call sleep_ms(job.work_ms).
     *   4. Call pool_free(ptr, job.id).
     */
    
    worker_arg_t *wa = (worker_arg_t *)arg;
    job_queue_t  *q  = wa->queue;
    job_t job;

    while (jq_pop(q, &job)) {
        void *ptr = pool_alloc(job.need, job.id);

        pthread_mutex_lock(&output_lock);
        printf("WORK job=%d ms=%d\n", job.id, job.work_ms);
        fflush(stdout);
        pthread_mutex_unlock(&output_lock);

        sleep_ms(job.work_ms);

        pool_free(ptr, job.id);
    }

    return NULL;
}

#include "poolsched.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Shared output lock — defined here, declared extern in poolsched.h */
/* ------------------------------------------------------------------ */

pthread_mutex_t output_lock = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------ */
/*  usage — print usage message and exit                              */
/* ------------------------------------------------------------------ */

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <num_workers> <job_file> -p first|next\n", prog);
    exit(1);
}

/* ------------------------------------------------------------------ */
/*  main                                                              */
/*                                                                    */
/*  1. Parse command-line arguments.                                  */
/*  2. Read the job file into an array of job_t.                      */
/*  3. Initialize the pool and job queue.                             */
/*  4. Submit all jobs (print SUBMIT lines, push to queue).           */
/*  5. Create num_workers threads running worker_main.                */
/*  6. Signal queue shutdown so workers exit after draining.          */
/*  7. Join all threads.                                              */
/*  8. Print the DONE line.                                           */
/*  9. Clean up.                                                      */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    /* ---- Parse arguments ---- */

    if (argc != 5 || strcmp(argv[3], "-p") != 0)
        usage(argv[0]);

    int num_workers = atoi(argv[1]);
    if (num_workers < 1 || num_workers > 8) {
        fprintf(stderr, "Error: num_workers must be 1-8\n");
        return 1;
    }

    const char *job_path = argv[2];

    alloc_policy_t policy;
    if (strcmp(argv[4], "first") == 0)
        policy = POLICY_FIRST_FIT;
    else if (strcmp(argv[4], "next") == 0)
        policy = POLICY_NEXT_FIT;
    else
        usage(argv[0]);

    /* ---- Read job file ---- */

    /* TODO:
     * 1. Open job_path for reading.  On failure, perror() and return 1.
     * 2. Read lines with fscanf(fp, "%zu %d", &need, &work_ms).
     * 3. For each line, validate: need must be > 0 and
     *    <= POOL_SIZE - HEADER_SIZE.
     * 4. Fill in a job_t (id = line number starting at 0, need, work_ms)
     *    and store it in a local array.
     * 5. Close the file.
     * 6. If no valid jobs were read, print an error and return 1.
     */

    job_t jobs[JOB_QUEUE_CAP];
    int num_jobs = 0;

    /* -- your file-reading code here -- */
    
    FILE *fp = fopen(job_path, "r");
    if (fp == NULL) {
        perror("Error opening job file");
        return 1;
    }

    size_t need;
    int work_ms;

    while (1) {
        int rc = fscanf(fp, "%zu %d", &need, &work_ms);

        if (rc == EOF)
            break;

        if (rc != 2) {
            fprintf(stderr, "Error: invalid job file format\n");
            fclose(fp);
            return 1;
        }

        if (need == 0 || need > POOL_SIZE - HEADER_SIZE || work_ms < 0) {
            fprintf(stderr, "Error: invalid job specification\n");
            fclose(fp);
            return 1;
        }

        if (num_jobs >= JOB_QUEUE_CAP) {
            fprintf(stderr, "Error: too many jobs\n");
            fclose(fp);
            return 1;
        }

        jobs[num_jobs].id      = num_jobs;
        jobs[num_jobs].need    = need;
        jobs[num_jobs].work_ms = work_ms;
        num_jobs++;
    }

    fclose(fp);

    if (num_jobs == 0) {
        fprintf(stderr, "Error: no valid jobs in file\n");
        return 1;
    } 

    /* ---- Initialize pool and queue ---- */

    pool_init(policy);

    job_queue_t queue;
    jq_init(&queue);

    /* ---- Submit all jobs ---- */

    /* TODO:
     * For each job, print a SUBMIT line and push it onto the queue.
     * Format: "SUBMIT job=%d need=%zu\n"
     * Flush stdout after all SUBMIT lines.
     */

	for (int i = 0; i < num_jobs; i++) {
    	    printf("SUBMIT job=%d need=%zu\n", jobs[i].id, jobs[i].need);
   	    jq_push(&queue, jobs[i]);
	}
	fflush(stdout);

    /* ---- Spawn worker threads ---- */

    /* TODO:
     * 1. Create num_workers threads.  Each thread runs worker_main.
     * 2. Pass a worker_arg_t to each thread with the queue pointer
     *    and the worker's id.
     * 3. On pthread_create failure, perror() and return 1.
     *
     * Hint: declare arrays for pthread_t and worker_arg_t of size 8
     * (the max number of workers).
     */
    
    pthread_t threads[8];
    worker_arg_t args[8];

    for (int i = 0; i < num_workers; i++) {
        args[i].queue     = &queue;
        args[i].worker_id = i;
        int rc = pthread_create(&threads[i], NULL, worker_main, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "Error: pthread_create failed: %s\n", strerror(rc));
            return 1;
        }
    }

    /* ---- Wait for completion ---- */

    /* TODO:
     * 1. Call jq_shutdown() so workers exit after the queue drains.
     * 2. Join all worker threads.
     * 3. Print the DONE line: "DONE completed=%d\n"
     * 4. Flush stdout.
     */
	
    jq_shutdown(&queue);

    for (int i = 0; i < num_workers; i++)
        pthread_join(threads[i], NULL);

    printf("DONE completed=%d\n", num_jobs);
    fflush(stdout);


    /* ---- Cleanup ---- */

    jq_destroy(&queue);
    pool_destroy();

    return 0;
}

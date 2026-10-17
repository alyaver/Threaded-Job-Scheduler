#include "poolsched.h"
#include <string.h>

void jq_init(job_queue_t *q)
{
    memset(q, 0, sizeof(*q));
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
}

void jq_push(job_queue_t *q, job_t job)
{
    pthread_mutex_lock(&q->lock);
    /* Block if full (should not happen with JOB_QUEUE_CAP=256). */
    q->buf[q->tail] = job;
    q->tail = (q->tail + 1) % JOB_QUEUE_CAP;
    q->count++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

int jq_pop(job_queue_t *q, job_t *out)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->shutdown)
        pthread_cond_wait(&q->cond, &q->lock);

    if (q->count == 0 && q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        return 0;
    }

    *out = q->buf[q->head];
    q->head = (q->head + 1) % JOB_QUEUE_CAP;
    q->count--;
    pthread_mutex_unlock(&q->lock);
    return 1;
}

void jq_shutdown(job_queue_t *q)
{
    pthread_mutex_lock(&q->lock);
    q->shutdown = 1;
    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

void jq_destroy(job_queue_t *q)
{
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
}

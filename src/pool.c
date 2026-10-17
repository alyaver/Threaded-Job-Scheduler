#include "poolsched.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Pool and synchronization state                                    */
/*                                                                    */
/*  pool[]     — the fixed memory region that your allocator manages. */
/*               Every allocation comes from inside this array.       */
/*  free_list  — head of the address-ordered linked list of blocks.   */
/*  last_alloc — roving pointer for next-fit (points to the block     */
/*               where the last successful allocation occurred).      */
/*               Ignored when policy is first-fit.                    */
/*  pool_lock  — protects all pool/free-list state.                   */
/*  pool_cond  — workers wait here when allocation fails; signaled    */
/*               when any block is freed.                             */
/* ------------------------------------------------------------------ */

static char pool[POOL_SIZE];

static block_header_t *free_list;
static block_header_t *last_alloc;
static alloc_policy_t  policy;

static pthread_mutex_t pool_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  pool_cond = PTHREAD_COND_INITIALIZER;

/* ------------------------------------------------------------------ */
/*  Helpers — provided, do not modify                                 */
/* ------------------------------------------------------------------ */

/* Return a pointer to the payload (usable memory) of a block. */
static inline void *payload_of(block_header_t *b)
{
    return (char *)b + HEADER_SIZE;
}

/* Return the header pointer for a given payload pointer. */
static inline block_header_t *header_of(void *ptr)
{
    return (block_header_t *)((char *)ptr - HEADER_SIZE);
}

/* Return the byte offset of ptr from the start of the pool. */
size_t pool_offset(void *ptr)
{
    return (size_t)((char *)ptr - pool);
}

/* ------------------------------------------------------------------ */
/*  pool_init — initialize the pool as one large free block           */
/*                                                                    */
/*  After this call the free list should contain exactly one block    */
/*  whose payload size is POOL_SIZE - HEADER_SIZE.                    */
/* ------------------------------------------------------------------ */

void pool_init(alloc_policy_t p)
{
    /* TODO: store the policy */

    /* TODO: cast the start of pool[] to a block_header_t pointer
     *       and set it up as a single free block covering the
     *       entire pool.  Set free_list and last_alloc to point
     *       to this block. */
    policy = p;

    block_header_t *block = (block_header_t *)pool;
    block->size = POOL_SIZE - HEADER_SIZE;
    block->free = 1;
    block->next = NULL;

    free_list  = block;
    last_alloc = block;
}

/* ------------------------------------------------------------------ */
/*  pool_destroy — clean up synchronization primitives                */
/* ------------------------------------------------------------------ */



void pool_destroy(void)
{
    /* TODO: destroy pool_lock and pool_cond */
    pthread_mutex_destroy(&pool_lock);
    pthread_cond_destroy(&pool_cond);
}

/* ------------------------------------------------------------------ */
/*  split_if_possible — split a block after allocation                */
/*                                                                    */
/*  If the block is large enough that the leftover space can hold a   */
/*  new header plus at least 1 byte of payload (>= MIN_BLOCK_SIZE),  */
/*  create a new free block from the remainder and link it into the   */
/*  list right after the current block.                               */
/*                                                                    */
/*  If the remainder is too small, do nothing (the caller gets the    */
/*  full block — this is internal fragmentation).                     */
/* ------------------------------------------------------------------ */

static void split_if_possible(block_header_t *block, size_t requested)
{
    /* TODO:
     * 1. Compute remainder = block->size - requested.
     * 2. If remainder >= MIN_BLOCK_SIZE:
     *    a. Compute the address of the new block:
     *       (char *)payload_of(block) + requested
     *    b. Set new block's size = remainder - HEADER_SIZE
     *    c. Mark it free, link it: new->next = block->next
     *    d. Shrink original block: block->size = requested
     *    e. Link it in: block->next = new block
     */

    size_t remainder = block->size - requested;

    if (remainder >= MIN_BLOCK_SIZE) {
        block_header_t *newblock = (block_header_t *)((char *)payload_of(block) + requested);
        newblock->size = remainder - HEADER_SIZE;
        newblock->free = 1;
        newblock->next = block->next;

        block->size = requested;
        block->next = newblock;
    }
}

/* ------------------------------------------------------------------ */
/*  try_alloc — search the free list for a suitable block             */
/*                                                                    */
/*  First-fit: always start from free_list (the head).                */
/*  Next-fit:  start from last_alloc, wrap around to the head if      */
/*             needed.                                                */
/*                                                                    */
/*  When a suitable free block is found:                              */
/*    1. Call split_if_possible() to carve off unused space.          */
/*    2. Mark the block as allocated (free = 0).                      */
/*    3. For next-fit, update last_alloc to this block.               */
/*    4. Return payload_of(block).                                    */
/*                                                                    */
/*  If no block fits, return NULL.                                    */
/* ------------------------------------------------------------------ */

static void *try_alloc(size_t size)
{
    /* TODO:
     * 1. Choose the starting block based on policy.
     *    - first-fit: start = free_list
     *    - next-fit:  start = last_alloc (or free_list if NULL)
     *
     * 2. Walk from start to end of list, looking for a free block
     *    with block->size >= size.
     *
     * 3. For next-fit only: if you reached the end without finding
     *    a block and you didn't start at the head, wrap around —
     *    walk from free_list up to (but not including) start.
     *
     * 4. If found: split, mark allocated, update last_alloc if
     *    next-fit, return payload pointer.
     *
     * 5. If not found: return NULL.
     */

     
    block_header_t *start = (policy == POLICY_FIRST_FIT) ? free_list : 
                            (last_alloc ? last_alloc : free_list);
    block_header_t *block;

    for (block = start; block != NULL; block = block->next) {
        if (block->free && block->size >= size) {
            split_if_possible(block, size);
            block->free = 0;
            if (policy == POLICY_NEXT_FIT)
                last_alloc = block;
            return payload_of(block);
        }
    }

    if (policy == POLICY_NEXT_FIT && start != free_list) {
        for (block = free_list; block != NULL && block != start; block = block->next) {
            if (block->free && block->size >= size) {
                split_if_possible(block, size);
                block->free = 0;
                last_alloc = block;
                return payload_of(block);
            }
        }
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/*  do_free — mark a block as free and coalesce with neighbors        */
/*                                                                    */
/*  Steps:                                                            */
/*    1. Find the header from the payload pointer (use header_of).    */
/*    2. Mark the block free.                                         */
/*    3. Print the FREE line.                                         */
/*    4. Coalesce forward: if block->next is free, merge it into      */
/*       this block.  Print a COALESCE line.                          */
/*    5. Coalesce backward: walk from free_list to find the block     */
/*       that precedes this one.  If that predecessor is free,        */
/*       merge this block into it.  Print a COALESCE line.            */
/*                                                                    */
/*  When merging block B into block A (A is earlier in memory):       */
/*    - A->size += HEADER_SIZE + B->size  (absorb B's header too)     */
/*    - A->next = B->next  (skip B in the list)                       */
/*                                                                    */
/*  Important: if last_alloc points to a block being absorbed,        */
/*  update last_alloc to point to the surviving block so that         */
/*  next-fit doesn't dangle.                                          */
/*                                                                    */
/*  All output in this function must be protected by output_lock.     */
/*  Lock output_lock once at the start, print FREE and any COALESCE   */
/*  lines, then unlock.  (The caller already holds pool_lock.)        */
/* ------------------------------------------------------------------ */

static void do_free(void *ptr, int job_id)
{
    /* TODO */
    block_header_t *block = header_of(ptr);
    block->free = 1;

    pthread_mutex_lock(&output_lock);

    printf("FREE job=%d addr=%zu\n", job_id, pool_offset(ptr));

    if (block->next != NULL && block->next->free) {
        block_header_t *absorbed = block->next;
        block->size += HEADER_SIZE + absorbed->size;
        block->next  = absorbed->next;

        if (last_alloc == absorbed)
            last_alloc = block;

        printf("COALESCE addr=%zu size=%zu\n",
               pool_offset(payload_of(block)), block->size);
    }

    block_header_t *prev = NULL;
    for (block_header_t *cur = free_list; cur != NULL && cur != block; cur = cur->next)
        prev = cur;

    if (prev != NULL && prev->free) {
        prev->size += HEADER_SIZE + block->size;
        prev->next  = block->next;

        if (last_alloc == block)
            last_alloc = prev;

        printf("COALESCE addr=%zu size=%zu\n",
               pool_offset(payload_of(prev)), prev->size);
    }

    fflush(stdout);
    pthread_mutex_unlock(&output_lock);
}

/* ------------------------------------------------------------------ */
/*  pool_alloc — thread-safe allocation with wait-on-failure          */
/*                                                                    */
/*  1. Lock pool_lock.                                                */
/*  2. Try to allocate with try_alloc().                              */
/*  3. If try_alloc returns NULL (no block large enough):             */
/*     a. Print a WAIT line (lock output_lock around the print).      */
/*     b. Wait on pool_cond (this releases pool_lock until signaled). */
/*     c. When woken, loop back to step 2.                            */
/*  4. On success, print an ALLOC line (lock output_lock).            */
/*  5. Unlock pool_lock.                                              */
/*  6. Return the pointer.                                            */
/* ------------------------------------------------------------------ */

void *pool_alloc(size_t size, int job_id)
{
    /* TODO */
    pthread_mutex_lock(&pool_lock);

    void *ptr = try_alloc(size);

    while (ptr == NULL) {
        pthread_mutex_lock(&output_lock);
        printf("WAIT job=%d need=%zu\n", job_id, size);
        fflush(stdout);
        pthread_mutex_unlock(&output_lock);

        pthread_cond_wait(&pool_cond, &pool_lock);
        ptr = try_alloc(size);
    }

    pthread_mutex_lock(&output_lock);
    printf("ALLOC job=%d addr=%zu size=%zu\n", job_id, pool_offset(ptr), size);
    fflush(stdout);
    pthread_mutex_unlock(&output_lock);

    pthread_mutex_unlock(&pool_lock);
    return ptr;
}

/* ------------------------------------------------------------------ */
/*  pool_free — thread-safe free with broadcast                       */
/*                                                                    */
/*  1. Lock pool_lock.                                                */
/*  2. Call do_free() (which prints FREE/COALESCE lines).             */
/*  3. Broadcast on pool_cond so any waiting workers can retry.       */
/*  4. Unlock pool_lock.                                              */
/* ------------------------------------------------------------------ */

void pool_free(void *ptr, int job_id)
{
    pthread_mutex_lock(&pool_lock);
    do_free(ptr, job_id);
    pthread_cond_broadcast(&pool_cond);
    pthread_mutex_unlock(&pool_lock);
}

/* ------------------------------------------------------------------ */
/*  print_pool — debug helper, provided, do not modify                */
/* ------------------------------------------------------------------ */

void print_pool(void)
{
    pthread_mutex_lock(&pool_lock);
    fprintf(stderr, "--- pool state ---\n");
    for (block_header_t *b = free_list; b; b = b->next) {
        fprintf(stderr, "  [offset=%5zu  size=%6zu  %s]\n",
                pool_offset(payload_of(b)), b->size,
                b->free ? "FREE" : "USED");
    }
    fprintf(stderr, "------------------\n");
    pthread_mutex_unlock(&pool_lock);
}

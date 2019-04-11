/*
 * interface for garbage collection
 */

#ifndef GARBAGECOLL_H_
#define GARBAGECOLL_H_

#include "common.h"
#include "ptst.h"

#define ADD_TO(_v,_x)                       \
do {                                        \
    unsigned long __val = (_v);             \
    while (!CAS(&(_v),__val,__val+(_x)))    \
        __val = (_v);                       \
} while ( 0 )

#define NUM_EPOCHS 3
#define MAX_HOOKS 4

#define CHUNKS_PER_ALLOC 1000
#define BLKS_PER_CHUNK 100
#define ALLOC_CHUNKS_PER_LIST 300

#define MAX_LEVELS 20
#define NUM_SIZES 1
#define NODE_SIZE 0

//#define MINIMAL_GC
//#define PROFILE_GC

/* Globals */

struct gc_chunk {
        struct gc_chunk *next;       /* chunk chaining */
        unsigned int i;              /* next entry in blk to use */
        void *blk[BLKS_PER_CHUNK];
};

/* Per-thread state */
struct gc_st {
        unsigned int epoch;     /* epoch seen by this thread */

        /* number of calls to gc_entry since last gc_reclaim() attempt */
        unsigned int entries_since_reclaim;

        /* used with gc_async_barrier() */
        void *async_page;
        int async_page_state;

        /* garbage lists */
        gc_chunk *garbage[NUM_EPOCHS][NUM_SIZES];
        gc_chunk *garbage_tail[NUM_EPOCHS][NUM_SIZES];
        gc_chunk *chunk_cache;

        /* local allocation lists */
        gc_chunk *alloc[NUM_SIZES];
        unsigned int alloc_chunks[NUM_SIZES];

        /* hook pointer lists */
        gc_chunk *hook[NUM_EPOCHS][MAX_HOOKS];
};


typedef struct gc_st gc_st;
//#include "skiplist.h"

typedef struct gc_chunk gc_chunk;
/* Initialise GC section of per-thread state */
gc_st* gc_init(void);

int gc_add_allocator(int alloc_size);
void gc_remove_allocator(int alloc_id);

void* gc_alloc(ptst_t *ptst, int alloc_id);
void gc_free(ptst_t *ptst, void *p, int alloc_id);
void gc_free_unsafe(ptst_t *ptst, void *p, int alloc_id);

/* Hook registry - allows users to hook in their own epoch-delay lists */
typedef void (*gc_hookfn)(ptst_t*, void*);
int gc_add_hook(gc_hookfn hookfn);
void gc_remove_hook(int hookid);
void gc_hooklist_addptr(ptst_t *ptst, void *ptr, int hookid);

/* Per-thread entry/exit from critical regions */
void gc_enter(ptst_t* ptst);
void gc_exit(ptst_t* ptst);

/* Initialisation of GC */
void gc_subsystem_init(void);
void gc_subsystem_destroy(void);

#endif /* GARBAGECOLL_H_ */

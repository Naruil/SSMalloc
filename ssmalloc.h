#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sched.h>
#include <time.h>

#include <assert.h>
#include <execinfo.h>
#include <signal.h>

#include "atomic.h"
#include "bitops.h"
#include "queue.h"
#include "double-list.h"
#include "counted-list.h"
#include "cpuid.h"

/* Machine related */
#define PAGE_SIZE           (4096)
#define SUPER_PAGE_SIZE     (4*1024*1024)
#define CACHE_LINE_SIZE     (64)
#define DEFAULT_BLOCK_CLASS (100)
#define MAX_CORE_ID         (8)

/* Configurations */
#define CHUNK_DATA_SIZE     (16*PAGE_SIZE)
#define CHUNK_SIZE          (CHUNK_DATA_SIZE+sizeof(dchunk_t))
#define CHUNK_MASK          (~(CHUNK_SIZE-1))
#define _MEM_START          ((void*)0x600000000000)
#define RAW_POOL_START      ((void*)((0x600000000000/CHUNK_SIZE+1)*CHUNK_SIZE))
#define RAW_POOL_TOP        ((void*)0x700000000000)
#define MAX_FREE_SIZE       (4*1024*1024)
#define MAX_FREE_CHUNK      (MAX_FREE_SIZE/CHUNK_SIZE)

#define BLOCK_BUF_CNT       (16)

/* Other */
#define LARGE_CLASS         (100)
#define DUMMY_CLASS         (101)
#define ROUNDUP(x,n)        ((x+n-1)&(~(n-1)))
#define ROUNDDOWN(x,n)      (((x-n)&(~(n-1)))+1)
#define PAGE_ROUNDUP(x)     (ROUNDUP((uintptr_t)x,PAGE_SIZE))
#define PAGE_ROUNDDOWN(x)   (ROUNDDOWN((uintptr_t)x,PAGE_SIZE))
#define DCH                 sizeof(dchunk_t)

/* Other Macros */
#define ALLOC_UNIT (4 * 1024 * 1024)
#define CACHE_ALIGN __attribute__ ((aligned (CACHE_LINE_SIZE)))
#define ACTIVE ((void*)1)
#define THREAD_LOCAL __attribute__ ((tls_model ("initial-exec"))) __thread

#define likely(x)       __builtin_expect(!!(x),1)
#define unlikely(x)     __builtin_expect(!!(x),0)

/* Multi consumer queue */
#define queue_init(head)\
    lf_lifo_queue_init(head)
#define queue_put(head,elem)\
    lf_lifo_enqueue(head,elem)
#define queue_fetch(head)\
    lf_lifo_dequeue(head)
typedef lf_lifo_queue_t Queue;

/* Single consumer queue */
#define fast_queue_init(head)\
    lf_lifo_queue_init_nABA(head)
#define fast_queue_put(head,elem)\
    lf_lifo_enqueue_nABA(head,elem)
#define fast_queue_fetch(head)\
    lf_lifo_dequeue_nABA(head)
#define fast_queue_chain_fetch(head)\
    lf_lifo_chain_dequeue_nABA(head)
typedef volatile struct queue_elem_t *FastQueue;

/* Sequencial queue */
#define seq_queue_init(head)\
    seq_lifo_queue_init(head)
#define seq_queue_put(head,elem)\
    seq_lifo_enqueue(head,elem)
#define seq_queue_fetch(head)\
    seq_lifo_dequeue(head)
typedef struct queue_elem_t *SeqQueue;

/* Type definations */
typedef enum {
    UNINITIALIZED,
    READY
} init_state;

typedef enum {
    FOREGROUND,
    BACKGROUND,
    FULL
} dchunk_state;

typedef struct lheap_s lheap_t;
typedef struct gpool_s gpool_t;
typedef struct dchunk_s dchunk_t;
typedef struct chunk_s chunk_t;
typedef struct obj_buf_s obj_buf_t;

typedef double_list_t LinkedList;
typedef double_list_elem_t LinkedListElem;

struct large_header {
    CACHE_ALIGN uint32_t size;
    CACHE_ALIGN lheap_t * owner;
};

struct chunk_s {
    CACHE_ALIGN LinkedListElem active_link;
    uint32_t numa_node;
};

/* Data chunk header */
struct dchunk_s {
    /* Local Area */
    CACHE_ALIGN LinkedListElem active_link;
    uint32_t numa_node;

    /* Read Area */
     CACHE_ALIGN lheap_t * owner;
    uint32_t size_cls;

    /* Local Write Area */
     CACHE_ALIGN dchunk_state state;
    uint32_t free_blk_cnt;
    uint32_t blk_cnt;
    SeqQueue free_head;
    dchunk_t *next_dc;
    uint32_t block_size;
    char *free_mem;

    /* Remote Write Area */
     CACHE_ALIGN FastQueue * remote_free_head;
};

struct gpool_s {
    pthread_mutex_t lock;
    volatile char *pool_start;
    volatile char *pool_end;
    volatile char *free_start;
    Queue free_dc_head[MAX_CORE_ID];
    Queue free_lh_head[MAX_CORE_ID];
    Queue released_dc_head[MAX_CORE_ID];
};

struct obj_buf_s {
    void *dc;
    void *first;
    SeqQueue free_head;
    int count;
};

/* Per-thread data chunk pool */
struct lheap_s {
    CACHE_ALIGN LinkedListElem active_link;
    uint32_t numa_node;
    SeqQueue free_head;
    uint32_t free_cnt;

    dchunk_t *foreground[DEFAULT_BLOCK_CLASS];
    LinkedList background[DEFAULT_BLOCK_CLASS];
    dchunk_t dummy_chunk;
    obj_buf_t block_bufs[BLOCK_BUF_CNT];

     CACHE_ALIGN FastQueue need_gc[DEFAULT_BLOCK_CLASS];
};

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

void *malloc(size_t __size);
void *realloc(void *__ptr, size_t __size);
void free(void *__ptr);

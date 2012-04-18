#ifndef __QUEUE_H_
#define __QUEUE_H_

#include "atomic.h"
#include <stdlib.h>

#define CACHE_LINE_SIZE     (64)
#define CACHE_ALIGN __attribute__ ((aligned (CACHE_LINE_SIZE)))

#define ABA_ADDR_BIT    (48)
#define ABA_ADDR_MASK   ((1L<<ABA_ADDR_BIT)-1)
#define ABA_COUNT_MASK  (~ABA_ADDR_MASK)
#define ABA_COUNT_ONE   (1L<<ABA_ADDR_BIT)
#define ABA_ADDR(e)     ((void*)((unsigned long long)(e) & ABA_ADDR_MASK))
#define ABA_COUNT(e)    ((unsigned long long)(e) & ABA_COUNT_MASK)

typedef struct {
    CACHE_ALIGN volatile unsigned long long head;
} queue_head_t;

typedef void *seq_queue_head_t;
 
/* Multi-Consumer LIFO Queue */

static inline void mc_queue_init(queue_head_t *queue)
{
    queue->head = 0;
}

static inline void mc_enqueue(queue_head_t *queue, void *element)
{
    unsigned long long old_head;
    unsigned long long new_head;

    while(1) {
        old_head = queue->head;
        *(unsigned long long*)element = (unsigned long long )ABA_ADDR(old_head);
        new_head = (unsigned long long)element;
        new_head |= ABA_COUNT(old_head) + ABA_COUNT_ONE;
        if (compare_and_swap64(&queue->head, old_head, new_head)) {
            return;
        }
    }
}

static inline void *mc_dequeue(queue_head_t *queue)
{
    unsigned long long old_head;
    unsigned long long new_head;
    void* old_addr;

    while(1) {
        old_head = queue->head;
        old_addr = ABA_ADDR(old_head);
        if(old_addr == NULL) {
            return NULL;
        }
        new_head = *(unsigned long long*)old_addr;
        new_head |= ABA_COUNT(old_head) + ABA_COUNT_ONE;
        if (compare_and_swap64(&queue->head, old_head, new_head)) {
            return old_addr;
        }
    }
}

/* Single-Consumer LIFO Queue */

static inline void sc_queue_init(queue_head_t *queue)
{
    queue->head = 0;
}

static inline void sc_enqueue(queue_head_t *queue, void *element)
{
    unsigned long long old_head;
    unsigned long long new_head;

    while(1) {
        old_head = queue->head;
        *(unsigned long long*)element = old_head;
        new_head = (unsigned long long)element;
        if (compare_and_swap64(&queue->head, old_head, new_head)) {
            return;
        }
    }
}

static inline void *sc_dequeue(queue_head_t *queue)
{
    unsigned long long old_head;
    unsigned long long new_head;

    while(1) {
        old_head = queue->head;
        if(old_head == 0) {
            return NULL;
        }
        new_head = *(unsigned long long*)old_head;
        if (compare_and_swap64(&queue->head, old_head, new_head)) {
            return (void*)old_head;
        }
    }
}

static inline void *sc_chain_dequeue(queue_head_t *queue)
{
    unsigned long long old_head;
    while(1) {
        old_head = queue->head;
        if(old_head == 0) {
            return NULL;
        }
        if (compare_and_swap64(&queue->head, old_head, 0)) {
            return (void*)old_head;
        }
    }
}

/* Sequential LIFO Queue */

static inline void seq_queue_init(seq_queue_head_t *queue)
{
    *queue = NULL;
}

static inline void seq_enqueue(seq_queue_head_t *queue, void *element)
{
    *(void**)element = *queue;
    *queue = element;
}

static inline void *seq_dequeue(seq_queue_head_t *queue)
{
    void* old_head = *queue;
    if(old_head == NULL) {
        return NULL;
    }
    *queue = *(void**)old_head;
    return old_head;
}

#define seq_head(queue) (queue)

/* Counted Queue */
static inline void* counted_enqueue(queue_head_t *queue, void* elem) {
    unsigned long long old_head, new_head, prev;
    do {
        old_head = queue->head;
        *(unsigned long long*)elem = (unsigned long long)ABA_ADDR(old_head);
        new_head = (unsigned long long)elem;
        new_head |= ABA_COUNT(old_head) + ABA_COUNT_ONE;
        
    } while((prev=compare_and_swap64_out (
            &queue->head,
            old_head,
            new_head
    ))!=old_head);    

    return (void*)prev;
}

static inline void* counted_chain_enqueue(queue_head_t *queue, void* elems, void* tail, int cnt) {
    unsigned long long old_head, new_head, prev;
    do {
        old_head = queue->head;
        *(unsigned long long*)tail = (unsigned long long)ABA_ADDR(old_head);
        new_head = (unsigned long long)elems;
        new_head |= ABA_COUNT(old_head) + ABA_COUNT_ONE * cnt;

    } while((prev=compare_and_swap64_out (
            &queue->head,
            old_head,
            new_head
    ))!=old_head);    

    return (void*)prev;
}

static inline void* counted_chain_dequeue(queue_head_t* queue, uint32_t *count) {
    unsigned long long old_head;
	while(1) {
		old_head = *(unsigned long long*)queue;
		if (old_head == 0)
			return(NULL);
		if (compare_and_swap64(&queue->head, old_head, 0)) {
            *count = ABA_COUNT(old_head) >> ABA_ADDR_BIT;
			return(ABA_ADDR(old_head));
		}
	}
}

#endif

#define CL_ADDR_BIT    (48)

#define CL_ADDR_MASK   ((1L<<CL_ADDR_BIT)-1)
#define CL_COUNT_MASK  (~CL_ADDR_MASK)
#define CL_COUNT_ONE   (1L<<CL_ADDR_BIT)

#define  CL_ADDR(e)     ((void*)((unsigned long long)(e) & CL_ADDR_MASK))
#define  CL_COUNT(e)    ((unsigned long long)(e) & CL_COUNT_MASK)

struct counted_queue_elem_t {
	volatile struct queue_elem_t 	*next;
};

static __inline__ void* counted_enqueue(void* head, void* elem) {
    unsigned long long head_old, head_new, prev;
    do {
        head_old = *(unsigned long long*)head;
        ((struct counted_queue_elem_t *)elem)->next = CL_ADDR(head_old);
        head_new = CL_COUNT(head_old) + CL_COUNT_ONE;
        head_new |= (unsigned long long)elem;
    } while((prev=compare_and_swap64_out (
            head,
            head_old,
            head_new
    ))!=head_old);    

    return (void*)prev;
}

static __inline__ void* counted_chain_enqueue(void* head, void* queue, void* tail, int cnt) {
    unsigned long long head_old, head_new, prev;
    do {
        head_old = *(unsigned long long*)head;
        ((struct counted_queue_elem_t *)tail)->next = CL_ADDR(head_old);
        head_new = CL_COUNT(head_old) + CL_COUNT_ONE*cnt;
        head_new |= (unsigned long long)queue;
    } while((prev=compare_and_swap64_out (
            head,
            head_old,
            head_new
    ))!=head_old);    

    return (void*)prev;
}

static __inline__ void* counted_chain_dequeue(void* head, uint32_t *count) {
    unsigned long long head_old;
	while(1) {
		head_old = *(unsigned long long*)head;
		if (head_old == 0)
			return(NULL);
		if (compare_and_swap64(head, head_old, 0)) {
            *count = CL_COUNT(head_old) >> CL_ADDR_BIT;
			return(CL_ADDR(head_old));
		}
	}
}
#ifndef QUEUE_H
#define QUEUE_H
#include "list.h"

typedef struct queue *queue;

/* Struct for queue elements */
struct queue
{
	list head;
	list tail;
};

/* create an empty queue */
extern queue queue_create(void);

/* insert an element at the end of the queue */
extern void queue_enq(queue q, void *element);

/* delete the front element on the queue and return it */
extern void *queue_deq(queue q);

/* return a true value if and only if the queue is empty */
extern int queue_empty(queue q);

/* return a true value if and only if the queue is tehnically empty */
extern int queue_tehnically_empty(queue q);

#endif

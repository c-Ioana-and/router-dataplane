#ifndef _LIST_H_
#define _LIST_H_


typedef struct cell *list;

struct cell
{
  void *element;
  list next;
};

typedef struct queue_entry *queue_entry;

struct queue_entry {
    char* buf;
    int len;
};

extern list cons(void *element, list l);
extern list cdr_and_free(list l);

#endif /* _LIST_H_ */

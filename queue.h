#ifndef QUEUE_H
#define QUEUE_H

#define QUEUE_MAX 20

typedef struct {
    int data[QUEUE_MAX];
    int front, rear, size;
} Queue;

void q_init    (Queue *q);
int  q_empty   (Queue *q);
int  q_full    (Queue *q);
void q_enqueue (Queue *q, int pid);
int  q_dequeue (Queue *q);
int  q_peek    (Queue *q);
void q_remove  (Queue *q, int pid);   /* remove specific PID */
int  q_contains(Queue *q, int pid);
void q_print   (Queue *q, const char *label);

#endif

#include "queue.h"
#include <stdio.h>

void q_init(Queue *q) {
    q->front = 0;
    q->rear  = 0;
    q->size  = 0;
}

int q_empty(Queue *q) { return q->size == 0; }
int q_full (Queue *q) { return q->size == QUEUE_MAX; }

void q_enqueue(Queue *q, int pid) {
    if (q_full(q)) { fprintf(stderr, "Queue full!\n"); return; }
    q->data[q->rear] = pid;
    q->rear = (q->rear + 1) % QUEUE_MAX;
    q->size++;
}

int q_dequeue(Queue *q) {
    if (q_empty(q)) return -1;
    int pid  = q->data[q->front];
    q->front = (q->front + 1) % QUEUE_MAX;
    q->size--;
    return pid;
}

int q_peek(Queue *q) {
    return q_empty(q) ? -1 : q->data[q->front];
}

void q_remove(Queue *q, int pid) {
    int tmp[QUEUE_MAX];
    int n = 0;
    for (int i = 0; i < q->size; i++) {
        int val = q->data[(q->front + i) % QUEUE_MAX];
        if (val != pid) tmp[n++] = val;
    }
    q->front = 0;
    q->rear  = n;
    q->size  = n;
    for (int i = 0; i < n; i++) q->data[i] = tmp[i];
}

int q_contains(Queue *q, int pid) {
    for (int i = 0; i < q->size; i++)
        if (q->data[(q->front + i) % QUEUE_MAX] == pid) return 1;
    return 0;
}

void q_print(Queue *q, const char *label) {
    printf("  %-18s[ ", label);
    if (q_empty(q)) {
        printf("empty");
    } else {
        for (int i = 0; i < q->size; i++) {
            if (i) printf(", ");
            printf("P%d", q->data[(q->front + i) % QUEUE_MAX]);
        }
    }
    printf(" ]\n");
}

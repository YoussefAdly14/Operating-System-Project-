#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "queue.h"

/* Scheduler state */
typedef struct {
    SchedAlgo algo;
    Queue     readyQ;
    Queue     blockedQ;
    int       currentPID;    /* -1 = idle   */
    int       quantumLeft;   /* for RR/MLFQ */
    int       quantum;       /* configurable quantum size */

    /* MLFQ: multiple ready queues */
    Queue     mlfqQ[MLFQ_LEVELS];
} Scheduler;

extern Scheduler sched;

void sched_init  (SchedAlgo algo, int quantum);
void sched_admit (int pid, int clock);        /* add new PID to ready queue */
int  sched_pick  (int clock);                 /* choose next PID; -1 = idle */
void sched_tick  (int clock);                 /* called after each instruction */
void sched_print_queues(int clock);

/* MLFQ helpers */
void mlfq_enqueue(int pid, int level);
int  mlfq_pick   (void);

#endif

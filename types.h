#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Tuneable constants */
#define MAX_MEMORY       40
#define MAX_PROCESSES    10
#define MAX_STR          256
#define NUM_VARS         3      /* variable slots per process in memory */
#define PCB_SIZE         4      /* PCB occupies 4 memory words */
#define DEFAULT_QUANTUM  2      /* RR: instructions per time-slice */
#define MLFQ_LEVELS      4
#define NUM_PROGRAMS     3

/* Process state */
typedef enum {
    READY    = 0,
    RUNNING  = 1,
    BLOCKED  = 2,
    FINISHED = 3
} ProcessState;

static inline const char *state_str(ProcessState s) {
    switch (s) {
        case READY:    return "READY";
        case RUNNING:  return "RUNNING";
        case BLOCKED:  return "BLOCKED";
        case FINISHED: return "FINISHED";
        default:       return "UNKNOWN";
    }
}

/* Scheduling algorithm */
typedef enum { SCHED_RR = 0, SCHED_HRRN = 1, SCHED_MLFQ = 2 } SchedAlgo;

/* One word in main memory */
typedef struct {
    char name [MAX_STR];
    char value[MAX_STR];
    int  used;
} MemoryWord;

#endif /* TYPES_H */

#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"

/* Process table entry  */
typedef struct {
    int  pid;
    ProcessState state;
    int  pc;                    /* program counter = next instruction idx */
    int  lowerBound;            /* memory lower bound  (-1 if on disk)   */
    int  upperBound;            /* memory upper bound                     */
    int  numInstructions;       /* total instruction count                */
    int  remainingInstr;        /* instructions not yet executed          */
    int  arrivalTime;
    int  waitingTime;           /* cumulative cycles spent in READY       */
    int  lastReadyTime;         /* clock when it last became READY        */
    int  onDisk;                /* 1 = swapped out                        */
    char diskFile[MAX_STR];     /* path to disk-swap file                 */
    char programFile[MAX_STR];  /* original .txt file                     */
    int  queueLevel;            /* MLFQ queue level                       */
    int  active;                /* slot in use                            */
} ProcessEntry;

/*  Global process table*/
extern ProcessEntry proctab[MAX_PROCESSES];
extern int          numProcesses;

void proc_init(void);

/*
 * Create a process from a .txt program file.
 * Returns PID on success, -1 on failure.
 */
int proc_create(const char *programFile, int arrivalTime);

/* Swap a process out to disk to free its memory block. */
void proc_swap_out(int pid, int clock);

/* Swap a process back in from disk. */
void proc_swap_in(int pid, int clock);

/* Sync PCB fields from proctab → memory. */
void proc_sync_to_mem(int pid);

/* Set process state (updates both proctab and memory). */
void proc_set_state(int pid, ProcessState newState, int clock);

/* Update PC (proctab + memory). */
void proc_set_pc(int pid, int pc);

/* Pretty-print the process table. */
void proc_print_table(void);

#endif

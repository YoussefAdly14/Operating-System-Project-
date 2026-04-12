#ifndef PROCESS_H
#define PROCESS_H

#include "pcb.h"
#include "memory.h"

#define MAX_PROCESSES 10
#define MAX_INSTRUCTIONS 30

typedef struct {
    PCB pcb;
    int num_instructions;
} Process;

extern Process processTable[MAX_PROCESSES];
extern int processCount;

int createProcess(int pid, char *filename, int arrival_time);
void printProcess(int pid);

#endif
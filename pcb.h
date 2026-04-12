#ifndef PCB_H
#define PCB_H

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    FINISHED
} ProcessState;

typedef struct {
    int pid;
    ProcessState state;
    int program_counter;
    int memory_lower;
    int memory_upper;
} PCB;

#endif  
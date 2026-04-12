#include <stdio.h>
#include <string.h>
#include "process.h"

Process processTable[MAX_PROCESSES];
int processCount = 0;

int createProcess(int pid, char *filename, int arrival_time) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: could not open file %s\n", filename);
        return -1;
    }

    // Read instructions 
    char instructions[MAX_INSTRUCTIONS][MAX_WORD_LEN];
    int num_instructions = 0;
    while (fgets(instructions[num_instructions], MAX_WORD_LEN, file) != NULL) {

        instructions[num_instructions][strcspn(instructions[num_instructions], "\n")] = '\0';
        num_instructions++;
    }
    fclose(file);

    // try to allocate memory for this process
    int needed = 4 + 3 + num_instructions;
    int start = allocateProcess(needed);
    if (start == -1) {
        printf("Not enough memory for process %d\n", pid);
        return -1;
    }

    // write pcb into memory
    char buf[MAX_WORD_LEN];

    sprintf(buf, "%d", pid);
    writeMemory(start,     "pid",   buf);
    writeMemory(start + 1, "state", "READY");

    sprintf(buf, "%d", start + 7);
    writeMemory(start + 2, "pc", buf);

    sprintf(buf, "%d-%d", start, start + needed - 1);
    writeMemory(start + 3, "bounds", buf);

    // write instructions into memory 
    for (int i = 0; i < num_instructions; i++) {
        char key[MAX_WORD_LEN];
        sprintf(key, "instr_%d", i);
        writeMemory(start + 7 + i, key, instructions[i]);
    }

    // save process in process table
    processTable[processCount].pcb.pid            = pid;
    processTable[processCount].pcb.state          = READY;
    processTable[processCount].pcb.program_counter = start + 7;
    processTable[processCount].pcb.memory_lower   = start;
    processTable[processCount].pcb.memory_upper   = start + needed - 1;
    processTable[processCount].num_instructions   = num_instructions;
    processCount++;

    printf("Process %d created at memory [%d - %d]\n", pid, start, start + needed - 1);
    return 0;
}

void printProcess(int pid) {
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].pcb.pid == pid) {
            printf("\n--- Process %d ---\n", pid);
            printf("State: %d\n", processTable[i].pcb.state);
            printf("PC: %d\n", processTable[i].pcb.program_counter);
            printf("Memory: [%d - %d]\n",
                processTable[i].pcb.memory_lower,
                processTable[i].pcb.memory_upper);
            printf("Instructions: %d\n", processTable[i].num_instructions);
            return;
        }
    }
    printf("Process %d not found\n", pid);
}
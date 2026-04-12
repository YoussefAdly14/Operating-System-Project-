#include <stdio.h>
#include <string.h>
#include "memory.h"

MemoryWord memory[MEMORY_SIZE];

void clearMemory() {
    for (int i = 0; i < MEMORY_SIZE; i++) {
        memory[i].in_use = 0;
        memory[i].name[0] = '\0';
        memory[i].value[0] = '\0';
    }
}

int allocateProcess(int num_words) {
    int start = -1, count = 0;
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (!memory[i].in_use) {
            if (start == -1) start = i;
            count++;
            if (count == num_words) return start;
        } else {
            start = -1;
            count = 0;
        }
    }
    return -1;
}

void freeProcess(int start, int end) {
    for (int i = start; i <= end; i++) {
        memory[i].in_use = 0;
        memory[i].name[0] = '\0';
        memory[i].value[0] = '\0';
    }
}

void writeMemory(int index, char *name, char *value) {
    memory[index].in_use = 1;
    strncpy(memory[index].name, name, MAX_WORD_LEN - 1);
    memory[index].name[MAX_WORD_LEN - 1] = '\0';
    strncpy(memory[index].value, value, MAX_WORD_LEN - 1);
    memory[index].value[MAX_WORD_LEN - 1] = '\0';
}

int findInMemory(int start, int end, char *name, char *result) {
    for (int i = start; i <= end; i++) {
        if (memory[i].in_use && strcmp(memory[i].name, name) == 0) {
            strncpy(result, memory[i].value, MAX_WORD_LEN - 1);
            result[MAX_WORD_LEN - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

void printMemory() {
    printf("\n===== MEMORY =====\n");
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (memory[i].in_use) {
            printf("[%2d] %-20s = %s\n", i, memory[i].name, memory[i].value);
        } else {
            printf("[%2d] EMPTY\n", i);
        }
    }
    printf("==================\n");
}
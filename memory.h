#ifndef MEMORY_H
#define MEMORY_H

#include "pcb.h"

#define MEMORY_SIZE 40
#define MAX_WORD_LEN 256

typedef struct {
    char name[MAX_WORD_LEN];
    char value[MAX_WORD_LEN];
    int in_use;
} MemoryWord;

extern MemoryWord memory[MEMORY_SIZE];

void clearMemory();
int allocateProcess(int num_words);
void freeProcess(int start, int end);
void writeMemory(int index, char *name, char *value);
int findInMemory(int start, int end, char *name, char *result);
void printMemory();

#endif
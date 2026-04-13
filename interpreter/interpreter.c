#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter.h"
#include "memory.h"     // MemoryWord, findInMemory(), writeMemory(), memory[]
#include "process.h"    // Process, processTable[], processCount
#include "pcb.h"        // PCB, ProcessState
#include "mutex.h"      // semWait(), semSignal()

// ─────────────────────────────────────────────────────────────────────────────
// HELPER: getProcessIndex
// ─────────────────────────────────────────────────────────────────────────────
// processTable is indexed by insertion order, NOT by pid.
// e.g. processTable[0] might have pid=1, processTable[1] might have pid=2.
//
// So we NEVER do processTable[pid] — we always search for the right entry.
// This function returns the array index for a given pid.
// Returns -1 if not found.

static int getProcessIndex(int pid) {
    for (int i = 0; i < processCount; i++) {
        if (processTable[i].pcb.pid == pid) return i;
    }
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 1: RESOLVE VALUE
// ─────────────────────────────────────────────────────────────────────────────
// Every instruction argument is either:
//   (a) a variable name stored in this process's memory → return its value
//   (b) a raw literal like "5" or "hello"              → return it as-is
//
// We search only within this process's memory boundaries:
//   memory_lower → memory_upper  (set by createProcess in process.c)
//
// findInMemory(start, end, name, result) from memory.c does the search.
// It returns 1 if found and fills result[], or 0 if not found.
//
// IMPORTANT: returns pointer to a static buffer.
// Copy immediately if you need to call resolveValue again before using the result.

char* resolveValue(int processID, char* token) {
    int idx = getProcessIndex(processID);
    if (idx == -1) return token;   // process not found, treat as raw value

    int start = processTable[idx].pcb.memory_lower;
    int end   = processTable[idx].pcb.memory_upper;

    static char result[MAX_WORD_LEN];
    result[0] = '\0';

    if (findInMemory(start, end, token, result)) {
        return result;   // token was a variable name → return its value
    }
    return token;        // token was a raw literal → return as-is
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 2: STORE VARIABLE IN PROCESS MEMORY
// ─────────────────────────────────────────────────────────────────────────────
// Writes a variable into this process's memory region.
// Two passes:
//   1st pass: if variable already exists → overwrite it (re-assignment)
//   2nd pass: find a free slot           → write new variable
//
// Memory layout per process (from process.c / createProcess):
//   start+0  : pid
//   start+1  : state
//   start+2  : pc
//   start+3  : bounds
//   start+4  to start+6 : 3 variable slots  ← storeVariable writes here
//   start+7  onwards    : instructions
//
// We search the full range (memory_lower to memory_upper) because
// findInMemory and memory[] already track what is in_use.

static void storeVariable(int processID, char* varName, char* value) {
    int idx = getProcessIndex(processID);
    if (idx == -1) {
        printf("ERROR: storeVariable — process %d not found\n", processID);
        return;
    }

    int start = processTable[idx].pcb.memory_lower;
    int end   = processTable[idx].pcb.memory_upper;

    // Pass 1: overwrite existing variable
    for (int i = start; i <= end; i++) {
        if (memory[i].in_use && strcmp(memory[i].name, varName) == 0) {
            writeMemory(i, varName, value);
            return;
        }
    }

    // Pass 2: find a free slot for a new variable
    // Slots start+4 to start+6 are the 3 variable slots reserved by createProcess
    // We search start+4 to start+6 specifically to stay within variable area
    for (int i = start + 4; i <= start + 6; i++) {
        if (i > end) break;
        if (!memory[i].in_use) {
            writeMemory(i, varName, value);
            return;
        }
    }

    printf("ERROR: Process %d has no free variable slots (max 3 variables)\n", processID);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 3: INSTRUCTION HANDLERS
// ─────────────────────────────────────────────────────────────────────────────

// ── assign x y ───────────────────────────────────────────────────────────────
// Stores variable x = y in this process's memory.
// If y == "input": print prompt, read from keyboard, store that.
// If y is a variable name: resolve it first, then store its value.
// If y is a raw literal: store it directly.
//
// Project requirement:
//   "assign x y, where x is the variable and y is the value assigned.
//    If y is input, it first prints 'Please enter a value',
//    then the value is taken as input from the user."
//
// Mutex: userInput locked while reading from keyboard.

void handle_assign(int pid, char* varName, char* value) {
    char resolvedValue[MAX_WORD_LEN];

    if (strcmp(value, "input") == 0) {
        // Special keyword: ask the user to type a value
        semWait("userInput");
        printf("Please enter a value: ");
        fflush(stdout);
        scanf("%255s", resolvedValue);
        semSignal("userInput");
    } else {
        // Resolve in case value is itself a variable name
        strncpy(resolvedValue, resolveValue(pid, value), MAX_WORD_LEN - 1);
        resolvedValue[MAX_WORD_LEN - 1] = '\0';
    }

    storeVariable(pid, varName, resolvedValue);
}

// ── print x ──────────────────────────────────────────────────────────────────
// Resolves x and prints its value to the screen.
//
// Project requirement:
//   "print: to print the output on the screen. Example: print x"
//
// Mutex: userOutput locked while printing.

void handle_print(int pid, char* varName) {
    // Copy resolved value immediately — static buffer gets overwritten on next call
    char val[MAX_WORD_LEN];
    strncpy(val, resolveValue(pid, varName), MAX_WORD_LEN - 1);
    val[MAX_WORD_LEN - 1] = '\0';

    semWait("userOutput");
    printf("%s\n", val);
    fflush(stdout);
    semSignal("userOutput");
}

// ── printFromTo x y ──────────────────────────────────────────────────────────
// Resolves x and y as integers, prints every number from x to y inclusive.
//
// Project requirement:
//   "printFromTo: to print all numbers between 2 numbers.
//    Example: printFromTo x y"
//
// Mutex: userOutput locked for the whole loop — output is uninterrupted.

void handle_printFromTo(int pid, char* fromToken, char* toToken) {
    // Copy both resolved values before using them
    // (resolveValue uses a single static buffer — calling it twice without
    //  copying would make the second call overwrite the first result)
    char fromStr[MAX_WORD_LEN];
    char toStr[MAX_WORD_LEN];
    strncpy(fromStr, resolveValue(pid, fromToken), MAX_WORD_LEN - 1);
    fromStr[MAX_WORD_LEN - 1] = '\0';
    strncpy(toStr, resolveValue(pid, toToken), MAX_WORD_LEN - 1);
    toStr[MAX_WORD_LEN - 1] = '\0';

    int start = atoi(fromStr);
    int end   = atoi(toStr);

    semWait("userOutput");
    for (int i = start; i <= end; i++) {
        printf("%d\n", i);
    }
    fflush(stdout);
    semSignal("userOutput");
}

// ── writeFile x y ────────────────────────────────────────────────────────────
// Resolves x as filename, y as data, creates the file and writes data into it.
//
// Project requirement:
//   "writeFile x y, where x is the filename and y is the data."
//   "Assume that the file doesn't exist and should always be created."
//
// Mutex: file locked while creating and writing.

void handle_writeFile(int pid, char* filenameToken, char* dataToken) {
    // Copy both before any fopen call
    char filename[MAX_WORD_LEN];
    char data[MAX_WORD_LEN];
    strncpy(filename, resolveValue(pid, filenameToken), MAX_WORD_LEN - 1);
    filename[MAX_WORD_LEN - 1] = '\0';
    strncpy(data, resolveValue(pid, dataToken), MAX_WORD_LEN - 1);
    data[MAX_WORD_LEN - 1] = '\0';

    semWait("file");

    FILE* f = fopen(filename, "w");   // "w" always creates a fresh file
    if (f == NULL) {
        printf("ERROR: Process %d could not create file '%s'\n", pid, filename);
        semSignal("file");
        return;
    }
    fprintf(f, "%s\n", data);
    fclose(f);

    semSignal("file");
}

// ── readFile x ───────────────────────────────────────────────────────────────
// Resolves x as filename, reads all contents, prints them to screen.
//
// Project requirement:
//   "readFile: to read data from a file. Example: readFile x"
//   Program 3 uses this to print file contents to the screen.
//
// Mutex: file locked while reading.
//        userOutput locked while printing the result.

void handle_readFile(int pid, char* filenameToken) {
    char filename[MAX_WORD_LEN];
    strncpy(filename, resolveValue(pid, filenameToken), MAX_WORD_LEN - 1);
    filename[MAX_WORD_LEN - 1] = '\0';

    semWait("file");

    FILE* f = fopen(filename, "r");
    if (f == NULL) {
        printf("ERROR: Process %d could not open file '%s'\n", pid, filename);
        semSignal("file");
        return;
    }

    // Read entire file into buffer
    char buffer[1000];
    buffer[0] = '\0';
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';   // strip trailing newline
        strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);
        strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);
    }
    fclose(f);

    semSignal("file");

    // Print the file contents to screen
    semWait("userOutput");
    printf("%s", buffer);
    fflush(stdout);
    semSignal("userOutput");
}

// ── semWait x ────────────────────────────────────────────────────────────────
// Acquires the mutex for resource x.
// x must be one of: "userInput", "userOutput", "file"
//
// Project requirement:
//   "ONLY ONE process is allowed to use the resource at a time.
//    If a process requests the use of a resource while it is being used
//    by another process, it should be blocked and added to the blocked queue."
//
// The blocking logic lives entirely in mutex.c — we just call semWait().

void handle_semWait(char* resource) {
    semWait(resource);
}

// ── semSignal x ──────────────────────────────────────────────────────────────
// Releases the mutex for resource x.
// The unblocking/wakeup logic lives entirely in mutex.c.

void handle_semSignal(char* resource) {
    semSignal(resource);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION 4: MAIN EXECUTE FUNCTION
// ─────────────────────────────────────────────────────────────────────────────
// Single entry point called by the scheduler each clock cycle.
// Receives one raw instruction string and routes it to the right handler.
//
// HOW PARSING WORKS:
//   strtok() splits a string by spaces, returning one token per call.
//   First call:       strtok(copy, " \n") → first token (the command)
//   Subsequent calls: strtok(NULL, " \n") → next token each time
//   Returns NULL when no more tokens exist.
//
//   "writeFile fname data"
//   → "writeFile" | "fname" | "data" | NULL
//
// WHY WE COPY FIRST:
//   strtok modifies the string in-place by replacing spaces with '\0'.
//   The original instruction lives in memory[] and must never be modified.

void executeInstruction(int processID, char* instruction) {

    if (instruction == NULL || strlen(instruction) == 0) return;

    // Work on a copy — never touch the original in memory[]
    char copy[300];
    strncpy(copy, instruction, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    // Project output requirement: show which process is executing what
    printf("[Process %d] Executing: %s\n", processID, instruction);

    // First token = command name
    char* command = strtok(copy, " \n");
    if (command == NULL) return;   // blank line

    // ── assign x y ──────────────────────────────────────────────────────────
    if (strcmp(command, "assign") == 0) {
        char* varName = strtok(NULL, " \n");
        char* value   = strtok(NULL, " \n");
        if (!varName || !value) {
            printf("ERROR: assign requires 2 arguments\n");
            return;
        }
        handle_assign(processID, varName, value);
    }

    // ── print x ─────────────────────────────────────────────────────────────
    else if (strcmp(command, "print") == 0) {
        char* varName = strtok(NULL, " \n");
        if (!varName) {
            printf("ERROR: print requires 1 argument\n");
            return;
        }
        handle_print(processID, varName);
    }

    // ── printFromTo x y ─────────────────────────────────────────────────────
    else if (strcmp(command, "printFromTo") == 0) {
        char* from = strtok(NULL, " \n");
        char* to   = strtok(NULL, " \n");
        if (!from || !to) {
            printf("ERROR: printFromTo requires 2 arguments\n");
            return;
        }
        handle_printFromTo(processID, from, to);
    }

    // ── writeFile x y ───────────────────────────────────────────────────────
    else if (strcmp(command, "writeFile") == 0) {
        char* filename = strtok(NULL, " \n");
        char* data     = strtok(NULL, " \n");
        if (!filename || !data) {
            printf("ERROR: writeFile requires 2 arguments\n");
            return;
        }
        handle_writeFile(processID, filename, data);
    }

    // ── readFile x ──────────────────────────────────────────────────────────
    else if (strcmp(command, "readFile") == 0) {
        char* filename = strtok(NULL, " \n");
        if (!filename) {
            printf("ERROR: readFile requires 1 argument\n");
            return;
        }
        handle_readFile(processID, filename);
    }

    // ── semWait x ───────────────────────────────────────────────────────────
    else if (strcmp(command, "semWait") == 0) {
        char* resource = strtok(NULL, " \n");
        if (!resource) {
            printf("ERROR: semWait requires 1 argument\n");
            return;
        }
        handle_semWait(resource);
    }

    // ── semSignal x ─────────────────────────────────────────────────────────
    else if (strcmp(command, "semSignal") == 0) {
        char* resource = strtok(NULL, " \n");
        if (!resource) {
            printf("ERROR: semSignal requires 1 argument\n");
            return;
        }
        handle_semSignal(resource);
    }

    // ── Unknown command ──────────────────────────────────────────────────────
    else {
        printf("ERROR: Unknown command '%s' in process %d\n", command, processID);
    }
}

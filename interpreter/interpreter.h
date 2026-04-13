#ifndef INTERPRETER_H
#define INTERPRETER_H

// ─────────────────────────────────────────────────────────────────────────────
// interpreter.h
// ─────────────────────────────────────────────────────────────────────────────
// Depends on:
//   memory.h  → MemoryWord, findInMemory(), writeMemory(), memory[]
//   process.h → Process, processTable[], processCount, getProcessIndex()
//   pcb.h     → PCB, ProcessState
//   mutex.h   → semWait(), semSignal()
// ─────────────────────────────────────────────────────────────────────────────

// Called by the scheduler every clock cycle.
// processID  : pid of the currently running process
// instruction: one raw instruction string e.g. "assign x 5"
void executeInstruction(int processID, char* instruction);

// Resolves a token to its actual value:
//   if token is a variable name in this process's memory → return its stored value
//   otherwise → return token itself as a raw literal
// NOTE: returns a pointer to a static buffer — use or copy immediately.
char* resolveValue(int processID, char* token);

#endif

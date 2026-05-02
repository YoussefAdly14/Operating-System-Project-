#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "types.h"
#include "queue.h"

/*
 * Execute one instruction for the given PID.
 *
 * Returns:
 *   0  – instruction executed normally, process still running.
 *   1  – process has FINISHED.
 *   2  – process is now BLOCKED (due to semWait).
 *   3  – swap-in temporarily failed; process re-admitted to ready queue.
 */
int  interp_exec(int pid, Queue *readyQ, Queue *blockedQ, int clock);

/* Resolve a value token: if it's a known variable name, return its value,
   otherwise return the token itself. */
void interp_resolve(int pid, const char *token, char *out);

#endif

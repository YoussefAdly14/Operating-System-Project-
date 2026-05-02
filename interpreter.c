#include "interpreter.h"
#include "process.h"
#include "memory.h"
#include "mutex.h"
#include "scheduler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Find process entry*/
static ProcessEntry *find_proc(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (proctab[i].active && proctab[i].pid == pid)
            return &proctab[i];
    return NULL;
}

/* Resolve a token to its value */
void interp_resolve(int pid, const char *token, char *out) {
    ProcessEntry *p = find_proc(pid);
    if (!p) { strncpy(out, token, MAX_STR - 1); return; }
    if (!p->onDisk &&
        mem_get_var(p->lowerBound, p->upperBound, token, out))
        return;   /* found as variable */
    strncpy(out, token, MAX_STR - 1);   /* literal */
}

/* Execute one instruction */
int interp_exec(int pid, Queue *readyQ, Queue *blockedQ, int clock) {
    ProcessEntry *p = find_proc(pid);
    if (!p) return 1;

    /* Swap in if on disk */
    if (p->onDisk) {
        proc_swap_in(pid, clock);
        p = find_proc(pid);
        if (!p) return 1;
        if (p->onDisk) {
            /* No victim available this cycle; re-admit and try next cycle */
            printf("  [SWAP-IN] P%d could not be swapped in; rescheduling\n", pid);
            sched_admit(pid, clock);
            return 3;
        }
    }

    int pc = p->pc;
    if (pc >= p->numInstructions) {
        proc_set_state(pid, FINISHED, clock);
        return 1;
    }

    /* Fetch instruction from memory */
    char instr[MAX_STR];
    mem_get_instr(p->lowerBound, pc, instr);

    printf("  [P%d | clock=%d | pc=%d] Executing: \"%s\"\n",
           pid, clock, pc, instr);

    /* Tokenise */
    char copy[MAX_STR];
    strncpy(copy, instr, MAX_STR - 1);
    char *tokens[8];
    int   ntok = 0;
    char *tok  = strtok(copy, " \t");
    while (tok && ntok < 8) { tokens[ntok++] = tok; tok = strtok(NULL, " \t"); }
    if (ntok == 0) {
        /* if empty comment a line*/
        proc_set_pc(pid, pc + 1);
        p->remainingInstr--;
        return 0;
    }

    char *opcode = tokens[0];
    int   done   = 0;   /* return value */

    /* assign */
    if (strcmp(opcode, "assign") == 0 && ntok >= 3) {
        char *varName = tokens[1];
        char  val[MAX_STR];

        if (strcmp(tokens[2], "input") == 0) {
            /* system call: take input from user */
            printf("  [SYS] Please enter a value for '%s': ", varName);
            fflush(stdout);
            if (!fgets(val, MAX_STR, stdin))
                val[0] = '\0';
            val[strcspn(val, "\r\n")] = '\0';
        } else if (strcmp(tokens[2], "readFile") == 0 && ntok >= 4) {
            /* assign var readFile filename */
            char fname[MAX_STR];
            interp_resolve(pid, tokens[3], fname);
            FILE *f = fopen(fname, "r");
            if (!f) {
                fprintf(stderr, "  [SYS] readFile: cannot open '%s'\n", fname);
                snprintf(val, MAX_STR, "ERROR");
            } else {
                if (!fgets(val, MAX_STR, f)) val[0] = '\0';
                val[strcspn(val, "\r\n")] = '\0';
                fclose(f);
                printf("  [SYS] readFile '%s' → \"%s\"\n", fname, val);
            }
        } else {
            interp_resolve(pid, tokens[2], val);
        }
        mem_set_var(p->lowerBound, p->upperBound, varName, val);
        printf("  [MEM] %s = \"%s\"\n", varName, val);

    /*print */
    } else if (strcmp(opcode, "print") == 0 && ntok >= 2) {
        char val[MAX_STR];
        interp_resolve(pid, tokens[1], val);
        printf("  [OUT] P%d prints: %s\n", pid, val);

    /* printFromTo */
    } else if (strcmp(opcode, "printFromTo") == 0 && ntok >= 3) {
        char va[MAX_STR], vb[MAX_STR];
        interp_resolve(pid, tokens[1], va);
        interp_resolve(pid, tokens[2], vb);
        int from = atoi(va);
        int to   = atoi(vb);
        printf("  [OUT] P%d printFromTo %d to %d: ", pid, from, to);
        if (from <= to)
            for (int i = from; i <= to; i++) printf("%d ", i);
        else
            for (int i = from; i >= to; i--) printf("%d ", i);
        printf("\n");

    /* writeFile */
    } else if (strcmp(opcode, "writeFile") == 0 && ntok >= 3) {
        char fname[MAX_STR], data[MAX_STR];
        interp_resolve(pid, tokens[1], fname);
        interp_resolve(pid, tokens[2], data);
        FILE *f = fopen(fname, "w");
        if (!f) {
            fprintf(stderr, "  [SYS] writeFile: cannot create '%s'\n", fname);
        } else {
            fprintf(f, "%s\n", data);
            fclose(f);
            printf("  [SYS] writeFile '%s' ← \"%s\"\n", fname, data);
        }

    /* readFile*/
    } else if (strcmp(opcode, "readFile") == 0 && ntok >= 2) {
        char fname[MAX_STR], content[MAX_STR];
        interp_resolve(pid, tokens[1], fname);
        FILE *f = fopen(fname, "r");
        if (!f) {
            fprintf(stderr, "  [SYS] readFile: cannot open '%s'\n", fname);
        } else {
            if (!fgets(content, MAX_STR, f)) content[0] = '\0';
            content[strcspn(content, "\r\n")] = '\0';
            fclose(f);
            printf("  [SYS] readFile '%s' → \"%s\"\n", fname, content);
        }

    /* semWait */
    } else if (strcmp(opcode, "semWait") == 0 && ntok >= 2) {
        int blocked = mutex_wait(tokens[1], pid);
        if (blocked) {
            proc_set_state(pid, BLOCKED, clock);
            q_enqueue(blockedQ, pid);
            q_remove(readyQ, pid);
            /* advance PC so we don't re-execute semWait when unblocked */
            proc_set_pc(pid, pc + 1);
            p->remainingInstr--;
            return 2;   /* BLOCKED */
        }

    /*semSignal */
    } else if (strcmp(opcode, "semSignal") == 0 && ntok >= 2) {
        int woken = mutex_signal(tokens[1], pid);
        if (woken != -1) {
            q_remove(blockedQ, woken);
            sched_admit(woken, clock);   /* handles RR, HRRN, and MLFQ */
            printf("  [SCHED] P%d moved from BLOCKED → READY\n", woken);
        }

    } else {
        fprintf(stderr, "  [INTERP] Unknown opcode or bad args: '%s'\n", instr);
    }

    /* Advance PC */
    proc_set_pc(pid, pc + 1);
    p->remainingInstr--;

    /* Check if finished */
    if (p->pc >= p->numInstructions) {
        printf("  [FINISH] P%d has completed all instructions.\n", pid);
        proc_set_state(pid, FINISHED, clock);
        done = 1;
    }

    return done;
}

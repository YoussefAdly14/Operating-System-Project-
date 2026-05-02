#include "process.h"
#include "memory.h"
#include "scheduler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

ProcessEntry proctab[MAX_PROCESSES];
int          numProcesses = 0;

void proc_init(void) {
    memset(proctab, 0, sizeof(proctab));
    numProcesses = 0;
}

/* Helper: choose a victim process to swap out */
static int choose_victim(int excludePID) {
    /* Never evict the process currently on the CPU */
    int running = sched.currentPID;

    /* First try READY processes */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!proctab[i].active)               continue;
        if (proctab[i].pid == excludePID)     continue;
        if (proctab[i].pid == running)        continue; 
        if (proctab[i].onDisk)                continue;
        if (proctab[i].state == READY)        return proctab[i].pid;
    }
    /* Fall back to BLOCKED processes */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!proctab[i].active)               continue;
        if (proctab[i].pid == excludePID)     continue;
        if (proctab[i].pid == running)        continue; 
        if (proctab[i].onDisk)                continue;
        if (proctab[i].state == BLOCKED)      return proctab[i].pid;
    }
    return -1;
}

/* Swap out */
void proc_swap_out(int pid, int clock) {
    ProcessEntry *p = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (proctab[i].active && proctab[i].pid == pid) { p = &proctab[i]; break; }
    if (!p || p->onDisk) return;

    snprintf(p->diskFile, MAX_STR, "disk_p%d.txt", pid);
    mem_save_to_disk(p->lowerBound, p->upperBound, p->diskFile);
    printf("\n  [SWAP-OUT] P%d → disk ('%s')  (clock=%d)\n",
           pid, p->diskFile, clock);
    mem_free(p->lowerBound, p->upperBound);
    p->onDisk     = 1;
    p->lowerBound = -1;
    p->upperBound = -1;
  
}

/* Swap in */
void proc_swap_in(int pid, int clock) {
    ProcessEntry *p = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (proctab[i].active && proctab[i].pid == pid) { p = &proctab[i]; break; }
    if (!p || !p->onDisk) return;

    int size = PCB_SIZE + p->numInstructions + NUM_VARS;
    int lb   = mem_alloc(size);
    while (lb == -1) {
        printf("  [SWAP-IN] No room for P%d; evicting another process\n", pid);
        int victim = choose_victim(pid);
        if (victim == -1) {
            printf("  [SWAP-IN] No eviction candidate for P%d; will retry next cycle\n", pid);
            return;
        }
        proc_swap_out(victim, clock);
        lb = mem_alloc(size);
    }
    int ub = lb + size - 1;
    mem_load_from_disk(lb, ub, p->diskFile);
    /* Update bounds in memory */
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "%d-%d", lb, ub);
    strncpy(memory[lb + 3].value, buf, MAX_STR - 1);

    p->lowerBound = lb;
    p->upperBound = ub;
    p->onDisk     = 0;
    printf("\n  [SWAP-IN]  P%d ← disk ('%s')  [mem %d-%d]  (clock=%d)\n",
           pid, p->diskFile, lb, ub, clock);
    remove(p->diskFile);
}

/*  Create a new process*/
int proc_create(const char *programFile, int arrivalTime) {
    /* read instructions from file */
    FILE *f = fopen(programFile, "r");
    if (!f) { fprintf(stderr, "Cannot open %s\n", programFile); return -1; }

    char lines[MAX_STR * 3][MAX_STR];
    int  n = 0;
    char buf[MAX_STR];
    while (fgets(buf, MAX_STR, f) && n < MAX_STR * 3) {
        /* strip newline */
        buf[strcspn(buf, "\r\n")] = '\0';
        if (strlen(buf) == 0) continue;
        strncpy(lines[n++], buf, MAX_STR - 1);
    }
    fclose(f);

    if (n == 0) { fprintf(stderr, "Empty program: %s\n", programFile); return -1; }

    /* allocate memory; swap out processes until enough space is available */
    int size = PCB_SIZE + n + NUM_VARS;
    int lb   = mem_alloc(size);
    while (lb == -1) {
        printf("\n  [MEM-FULL] Need %d words. Swapping out a process...\n", size);
        int victim = choose_victim(-1);
        if (victim == -1) {
            fprintf(stderr, "No victim to swap! Cannot create process.\n");
            return -1;
        }
        proc_swap_out(victim, arrivalTime);
        lb = mem_alloc(size);
    }
    int ub = lb + size - 1;

    /* assign PID */
    int pid = numProcesses + 1;
    numProcesses++;

    /* find a free proctab slot */
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (!proctab[i].active) { slot = i; break; }
    if (slot == -1) { fprintf(stderr, "Process table full!\n"); return -1; }

    ProcessEntry *p = &proctab[slot];
    memset(p, 0, sizeof(*p));
    p->pid             = pid;
    p->state           = READY;
    p->pc              = 0;
    p->lowerBound      = lb;
    p->upperBound      = ub;
    p->numInstructions = n;
    p->remainingInstr  = n;
    p->arrivalTime     = arrivalTime;
    p->waitingTime     = 0;
    p->lastReadyTime   = arrivalTime;
    p->onDisk          = 0;
    p->queueLevel      = 0;
    p->active          = 1;
    strncpy(p->programFile, programFile, MAX_STR - 1);

    /* write PCB into memory */
    mem_pcb_write(lb, pid, (int)READY, 0, lb, ub);

    /* write instruction lines */
    for (int i = 0; i < n; i++) {
        char key[MAX_STR];
        snprintf(key, MAX_STR, "LINE_%d", i);
        mem_write(lb + PCB_SIZE + i, key, lines[i]);
    }

    /* initialise variable slots — kept used=1 so mem_alloc never overwrites them */
    for (int i = 0; i < NUM_VARS; i++) {
        mem_write(ub - NUM_VARS + 1 + i, "__VAR__", "");
    }

    printf("\n  [CREATE] P%d from '%s'  mem[%d-%d]  "
           "instructions=%d  (arrives at clock=%d)\n",
           pid, programFile, lb, ub, n, arrivalTime);
    return pid;
}

/*Sync proctab to memory */
void proc_sync_to_mem(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proctab[i].active && proctab[i].pid == pid) {
            ProcessEntry *p = &proctab[i];
            if (p->onDisk || p->lowerBound == -1) return;
            mem_pcb_write(p->lowerBound, p->pid,
                          (int)p->state, p->pc,
                          p->lowerBound, p->upperBound);
            return;
        }
    }
}

/* State change */
void proc_set_state(int pid, ProcessState newState, int clock) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proctab[i].active && proctab[i].pid == pid) {
            ProcessEntry *p = &proctab[i];
            /* Accumulate waiting time when leaving READY */
            if (p->state == READY && newState != READY) {
                p->waitingTime += clock - p->lastReadyTime;
            }
            /* Record when entering READY */
            if (newState == READY && p->state != READY) {
                p->lastReadyTime = clock;
            }
            p->state = newState;
            if (!p->onDisk && p->lowerBound != -1)
                mem_pcb_update_state(p->lowerBound, (int)newState);
            return;
        }
    }
}

/* PC update  */
void proc_set_pc(int pid, int pc) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proctab[i].active && proctab[i].pid == pid) {
            proctab[i].pc = pc;
            if (!proctab[i].onDisk && proctab[i].lowerBound != -1)
                mem_pcb_update_pc(proctab[i].lowerBound, pc);
            return;
        }
    }
}

/* print process table */
void proc_print_table(void) {
    printf("\n  %-4s %-9s %-4s %-10s %-8s %-6s %-6s\n",
           "PID","State","PC","Memory","Waiting","Instr","Disk");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!proctab[i].active) continue;
        ProcessEntry *p = &proctab[i];
        char mem_range[32];
        if (p->onDisk)
            snprintf(mem_range, sizeof(mem_range), "disk");
        else if (p->lowerBound == -1)
            snprintf(mem_range, sizeof(mem_range), "freed");
        else
            snprintf(mem_range, sizeof(mem_range), "%d-%d",
                     p->lowerBound, p->upperBound);
        printf("  %-4d %-9s %-4d %-10s %-8d %-6d %-6s\n",
               p->pid, state_str(p->state), p->pc,
               mem_range, p->waitingTime,
               p->remainingInstr, p->onDisk ? "yes" : "no");
    }
    printf("\n");
}

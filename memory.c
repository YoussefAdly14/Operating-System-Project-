#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

MemoryWord memory[MAX_MEMORY];


void mem_init(void) {
    for (int i = 0; i < MAX_MEMORY; i++) {
        memory[i].name[0]  = '\0';
        memory[i].value[0] = '\0';
        memory[i].used     = 0;
    }
}

/* Allocation: first-fit */
int mem_alloc(int size) {
    int count = 0, start = 0;
    for (int i = 0; i < MAX_MEMORY; i++) {
        if (!memory[i].used) {
            if (count == 0) start = i;
            count++;
            if (count == size) return start;
        } else {
            count = 0;
        }
    }
    return -1;   /* not enough contiguous space */
}

void mem_free(int lower, int upper) {
    for (int i = lower; i <= upper; i++) {
        memory[i].name[0]  = '\0';
        memory[i].value[0] = '\0';
        memory[i].used     = 0;
    }
}

/* Raw word write */
void mem_write(int idx, const char *name, const char *value) {
    strncpy(memory[idx].name,  name,  MAX_STR - 1);
    strncpy(memory[idx].value, value, MAX_STR - 1);
    memory[idx].used = 1;
}

/* PCB helpers*/

void mem_pcb_write(int lower, int pid, int state, int pc,
                   int lb, int ub) {
    char buf[MAX_STR];

    snprintf(buf, MAX_STR, "%d", pid);
    mem_write(lower + 0, "PCB_PID",    buf);

    snprintf(buf, MAX_STR, "%d", state);
    mem_write(lower + 1, "PCB_STATE",  buf);

    snprintf(buf, MAX_STR, "%d", pc);
    mem_write(lower + 2, "PCB_PC",     buf);

    snprintf(buf, MAX_STR, "%d-%d", lb, ub);
    mem_write(lower + 3, "PCB_BOUNDS", buf);
}

void mem_pcb_update_state(int lower, int state) {
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "%d", state);
    strncpy(memory[lower + 1].value, buf, MAX_STR - 1);
}

void mem_pcb_update_pc(int lower, int pc) {
    char buf[MAX_STR];
    snprintf(buf, MAX_STR, "%d", pc);
    strncpy(memory[lower + 2].value, buf, MAX_STR - 1);
}

void mem_pcb_read(int lower, int *pid, int *state, int *pc,
                  int *lb, int *ub) {
    *pid   = atoi(memory[lower + 0].value);
    *state = atoi(memory[lower + 1].value);
    *pc    = atoi(memory[lower + 2].value);
    sscanf(memory[lower + 3].value, "%d-%d", lb, ub);
}

/* ─── Instruction access ────────────────────────────────────────── */
void mem_get_instr(int lower, int instrIdx, char *out) {
    /* Instructions start at lower + PCB_SIZE */
    strncpy(out, memory[lower + PCB_SIZE + instrIdx].value, MAX_STR - 1);
}

/* ─── Variable access ────────────────────────────────────────────── */
/*  Variables occupy the last NUM_VARS words of the block.           */
/*  varBase = upper - NUM_VARS + 1                                   */
int mem_get_var(int lower, int upper, const char *name, char *out) {
    (void)lower;
    int varBase = upper - NUM_VARS + 1;
    for (int i = varBase; i <= upper; i++) {
        if (memory[i].used &&
            strcmp(memory[i].name, "__VAR__") != 0 &&
            strcmp(memory[i].name, name) == 0) {
            strncpy(out, memory[i].value, MAX_STR - 1);
            return 1;
        }
    }
    return 0;   /* not found */
}

void mem_set_var(int lower, int upper, const char *name, const char *value) {
    (void)lower;
    int varBase = upper - NUM_VARS + 1;
    /* update if already exists */
    for (int i = varBase; i <= upper; i++) {
        if (memory[i].used && strcmp(memory[i].name, name) == 0) {
            strncpy(memory[i].value, value, MAX_STR - 1);
            return;
        }
    }
    /* insert into first placeholder (__VAR__) slot */
    for (int i = varBase; i <= upper; i++) {
        if (memory[i].used && strcmp(memory[i].name, "__VAR__") == 0) {
            strncpy(memory[i].name,  name,  MAX_STR - 1);
            strncpy(memory[i].value, value, MAX_STR - 1);
            return;
        }
    }
    fprintf(stderr, "[MEM] No free variable slot for '%s'\n", name);
}

/*Disk swap */
void mem_save_to_disk(int lower, int upper, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) { perror("mem_save_to_disk"); return; }
    fprintf(f, "# Process memory dump: words %d-%d\n", lower, upper);
    for (int i = lower; i <= upper; i++) {
        fprintf(f, "%d|%d|%s|%s\n",
                i - lower,           /* relative index */
                memory[i].used,
                memory[i].name,
                memory[i].value);
    }
    fclose(f);
}

void mem_load_from_disk(int lower, int upper, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("mem_load_from_disk"); return; }
    char line[MAX_STR * 2];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        int rel, used;
        char name[MAX_STR], value[MAX_STR];
        name[0] = value[0] = '\0';
        if (sscanf(line, "%d|%d|%255[^|]|%255[^\n]",
                   &rel, &used, name, value) >= 3) {
            int idx = lower + rel;
            if (idx > upper) continue;
            memory[idx].used = used;
            strncpy(memory[idx].name,  name,  MAX_STR - 1);
            strncpy(memory[idx].value, value, MAX_STR - 1);
        }
    }
    fclose(f);
}

/* Display*/
void mem_print(int clock) {
    printf("\n┌─────────────────────────── MEMORY (clock=%d) "
           "───────────────────────────┐\n", clock);
    printf("│ %-4s  %-20s  %-30s  %-4s │\n",
           "Idx", "Name", "Value", "Used");
    printf("│─────────────────────────────────────────────────────────────────│\n");
    for (int i = 0; i < MAX_MEMORY; i++) {
        printf("│ %-4d  %-20s  %-30s  %-4s │\n",
               i,
               memory[i].used ? memory[i].name  : "---",
               memory[i].used ? memory[i].value : "---",
               memory[i].used ? "YES" : "no");
    }
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
}

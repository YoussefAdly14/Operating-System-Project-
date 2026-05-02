#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

/*  memory array  */
extern MemoryWord memory[MAX_MEMORY];

/* Lifecycle */
void mem_init(void);

/*  Allocation / deallocation */
/* Returns lower-bound index of a free contiguous block, or -1.       */
int  mem_alloc(int size);
void mem_free (int lower, int upper);

/* Raw word access*/
void mem_write(int idx, const char *name, const char *value);

/*  Variable access within a process block  */
/* varBase = first variable slot = lower + PCB_SIZE + numInstructions */
int  mem_get_var(int lower, int upper, const char *name, char *out);
void mem_set_var(int lower, int upper, const char *name, const char *value);

/* Instruction line access  */
/* instrBase = lower + PCB_SIZE */
void mem_get_instr(int lower, int instrIdx, char *out);

/* PCB helpers */
void mem_pcb_write(int lower, int pid, int state, int pc,
                   int lb, int ub);
void mem_pcb_update_state(int lower, int state);
void mem_pcb_update_pc   (int lower, int pc);
void mem_pcb_read(int lower, int *pid, int *state, int *pc,
                  int *lb, int *ub);

/*  Disk swap  */
void mem_save_to_disk  (int lower, int upper, const char *filename);
void mem_load_from_disk(int lower, int upper, const char *filename);

/* Display */
void mem_print(int clock);

#endif

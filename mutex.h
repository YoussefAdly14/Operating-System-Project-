#ifndef MUTEX_H
#define MUTEX_H

#include "types.h"
#include "queue.h"

#define NUM_MUTEXES 3

typedef struct {
    char  name[MAX_STR];
    int   locked;       /* 0 = free, 1 = locked */
    int   ownerPID;     /* -1 if free            */
    Queue blocked;      /* PIDs blocked on this mutex */
} Mutex;

extern Mutex mutexes[NUM_MUTEXES];

void mutex_init(void);

/* Returns index of mutex with that name, or -1 */
int  mutex_find(const char *name);

/*
 * semWait: if free → lock and return 0 (caller continues).
 *          if locked → block caller and return 1 (caller must reschedule).
 */
int  mutex_wait  (const char *name, int pid);

/*
 * semSignal: release the lock.
 *            If there are waiting processes, wake the first one.
 *            Returns the PID of the woken process, or -1.
 */
int  mutex_signal(const char *name, int pid);

void mutex_print(void);

#endif

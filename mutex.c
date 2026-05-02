#include "mutex.h"
#include <stdio.h>
#include <string.h>

Mutex mutexes[NUM_MUTEXES];

void mutex_init(void) {
    const char *names[NUM_MUTEXES] = {"userInput", "userOutput", "file"};
    for (int i = 0; i < NUM_MUTEXES; i++) {
        strncpy(mutexes[i].name, names[i], MAX_STR - 1);
        mutexes[i].locked   = 0;
        mutexes[i].ownerPID = -1;
        q_init(&mutexes[i].blocked);
    }
}

int mutex_find(const char *name) {
    for (int i = 0; i < NUM_MUTEXES; i++)
        if (strcmp(mutexes[i].name, name) == 0) return i;
    return -1;
}

int mutex_wait(const char *name, int pid) {
    int idx = mutex_find(name);
    if (idx == -1) {
        fprintf(stderr, "[MUTEX] Unknown mutex: '%s'\n", name);
        return 0;
    }
    Mutex *m = &mutexes[idx];
    if (!m->locked) {
        m->locked   = 1;
        m->ownerPID = pid;
        printf("  [MUTEX] P%d acquired '%s'\n", pid, name);
        return 0;      /* process continues */
    } else {
        printf("  [MUTEX] '%s' busy (owner P%d) → P%d BLOCKED\n",
               name, m->ownerPID, pid);
        q_enqueue(&m->blocked, pid);
        return 1;      /* caller must block this process */
    }
}

int mutex_signal(const char *name, int pid) {
    int idx = mutex_find(name);
    if (idx == -1) {
        fprintf(stderr, "[MUTEX] Unknown mutex: '%s'\n", name);
        return -1;
    }
    Mutex *m = &mutexes[idx];
    if (m->ownerPID != pid) {
        fprintf(stderr, "[MUTEX] P%d tried to release '%s' but doesn't own it!\n",
                pid, name);
        return -1;
    }
    if (!q_empty(&m->blocked)) {
        int next = q_dequeue(&m->blocked);
        m->ownerPID = next;
        printf("  [MUTEX] P%d released '%s' → P%d unblocked and acquires it\n",
               pid, name, next);
        return next;   /* caller must move next to READY */
    } else {
        m->locked   = 0;
        m->ownerPID = -1;
        printf("  [MUTEX] P%d released '%s' (now free)\n", pid, name);
        return -1;
    }
}

void mutex_print(void) {
    printf("  Mutexes:\n");
    for (int i = 0; i < NUM_MUTEXES; i++) {
        Mutex *m = &mutexes[i];
        printf("    %-12s : %s", m->name,
               m->locked ? "LOCKED" : "FREE  ");
        if (m->locked) printf(" (owner P%d)", m->ownerPID);
        if (!q_empty(&m->blocked)) {
            printf("  waiting=[");
            for (int j = 0; j < m->blocked.size; j++) {
                if (j) printf(",");
                printf("P%d", m->blocked.data[(m->blocked.front+j)%QUEUE_MAX]);
            }
            printf("]");
        }
        printf("\n");
    }
}

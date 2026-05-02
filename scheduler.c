#include "scheduler.h"
#include "process.h"
#include <stdio.h>
#include <float.h>

Scheduler sched;


void sched_init(SchedAlgo algo, int quantum) {
    sched.algo       = algo;
    sched.currentPID = -1;
    sched.quantumLeft= 0;
    sched.quantum    = quantum;
    q_init(&sched.readyQ);
    q_init(&sched.blockedQ);
    for (int i = 0; i < MLFQ_LEVELS; i++)
        q_init(&sched.mlfqQ[i]);
}

/*  Admit a new or returning process to the ready queue */
void sched_admit(int pid, int clock) {
    if (sched.algo == SCHED_MLFQ) {
        ProcessEntry *p = NULL;
        for (int i = 0; i < MAX_PROCESSES; i++)
            if (proctab[i].active && proctab[i].pid == pid) { p = &proctab[i]; break; }
        int lvl = p ? p->queueLevel : 0;
        mlfq_enqueue(pid, lvl);
    } else {
        if (!q_contains(&sched.readyQ, pid))
            q_enqueue(&sched.readyQ, pid);
    }
    proc_set_state(pid, READY, clock);
}

/* HRRN: pick the process with the highest response ratio */
static int pick_hrrn(int clock) {
    int   bestPID   = -1;
    float bestRatio = -FLT_MAX;

    for (int i = 0; i < sched.readyQ.size; i++) {
        int pid = sched.readyQ.data[(sched.readyQ.front + i) % QUEUE_MAX];
        ProcessEntry *p = NULL;
        for (int j = 0; j < MAX_PROCESSES; j++)
            if (proctab[j].active && proctab[j].pid == pid) { p = &proctab[j]; break; }
        if (!p) continue;

        int   burst  = (p->remainingInstr > 0) ? p->remainingInstr : 1;
        int   waited = p->waitingTime + (clock - p->lastReadyTime);
        float ratio  = (float)(waited + burst) / (float)burst;

        printf("    HRRN P%d: waited=%d remaining=%d ratio=%.3f\n",
               pid, waited, burst, ratio);

        if (ratio > bestRatio) {
            bestRatio = ratio;
            bestPID   = pid;
        }
    }
    return bestPID;
}

/* Round Robin: just take the front of the queue */
static int pick_rr(void) {
    return q_peek(&sched.readyQ);
}

/*  MLFQ */
void mlfq_enqueue(int pid, int level) {
    if (level < 0) level = 0;
    if (level >= MLFQ_LEVELS) level = MLFQ_LEVELS - 1;
    if (!q_contains(&sched.mlfqQ[level], pid))
        q_enqueue(&sched.mlfqQ[level], pid);
    /* keep track in readyQ too (for printing) */
    if (!q_contains(&sched.readyQ, pid))
        q_enqueue(&sched.readyQ, pid);
}

int mlfq_pick(void) {
    for (int i = 0; i < MLFQ_LEVELS; i++) {
        if (!q_empty(&sched.mlfqQ[i]))
            return q_peek(&sched.mlfqQ[i]);
    }
    return -1;
}

/* Quantum for MLFQ level i = 2^i (level 0 → 1, level 1 → 2, …) */
static int mlfq_quantum(int level) {
    int q = 1;
    for (int i = 0; i < level; i++) q *= 2;
    return q;
}

/* Main scheduling decision */
int sched_pick(int clock) {
    int pid = -1;

    if (q_empty(&sched.readyQ) && sched.algo != SCHED_MLFQ) return -1;

    switch (sched.algo) {
        case SCHED_HRRN:
            pid = pick_hrrn(clock);
            if (pid != -1) q_remove(&sched.readyQ, pid);
            break;

        case SCHED_RR:
            pid = pick_rr();
            if (pid != -1) q_dequeue(&sched.readyQ);
            sched.quantumLeft = sched.quantum;
            break;

        case SCHED_MLFQ:
            pid = mlfq_pick();
            if (pid != -1) {
                ProcessEntry *p = NULL;
                for (int i = 0; i < MAX_PROCESSES; i++)
                    if (proctab[i].active && proctab[i].pid == pid)
                    { p = &proctab[i]; break; }
                int lvl = p ? p->queueLevel : 0;
                q_dequeue(&sched.mlfqQ[lvl]);
                q_remove(&sched.readyQ, pid);
                sched.quantumLeft = mlfq_quantum(lvl);
            }
            break;
    }

    if (pid != -1) {
        sched.currentPID  = pid;
        proc_set_state(pid, RUNNING, clock);
        printf("\n  [SCHED] Scheduling P%d  (algo=%s, quantum=%d)\n",
               pid,
               sched.algo == SCHED_RR   ? "RR"   :
               sched.algo == SCHED_HRRN ? "HRRN" : "MLFQ",
               sched.quantumLeft);
    } else {
        sched.currentPID = -1;
    }
    return pid;
}

/* Called after each instruction to check preemption  */
void sched_tick(int clock) {
    if (sched.algo == SCHED_HRRN) return;   /* non-preemptive */

    /* Update waiting time for all READY processes */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proctab[i].active && proctab[i].state == READY)
            proctab[i].waitingTime++;
    }

    if (sched.currentPID == -1) return;

    sched.quantumLeft--;
    if (sched.quantumLeft <= 0) {
        int pid = sched.currentPID;
        ProcessEntry *p = NULL;
        for (int i = 0; i < MAX_PROCESSES; i++)
            if (proctab[i].active && proctab[i].pid == pid) { p = &proctab[i]; break; }

        if (!p || p->state == FINISHED || p->state == BLOCKED) return;

        printf("\n  [QUANTUM] P%d quantum expired → back to ready queue\n", pid);

        if (sched.algo == SCHED_MLFQ && p) {
            /* demote to lower queue */
            p->queueLevel++;
            if (p->queueLevel >= MLFQ_LEVELS)
                p->queueLevel = MLFQ_LEVELS - 1;
            printf("  [MLFQ] P%d demoted to queue level %d\n", pid, p->queueLevel);
            mlfq_enqueue(pid, p->queueLevel);
        } else {
            q_enqueue(&sched.readyQ, pid);
        }

        proc_set_state(pid, READY, clock);
        sched.currentPID  = -1;
        sched.quantumLeft = 0;
    }
}

/*  Print scheduler queues  */
void sched_print_queues(int clock) {
    char runBuf[16];
    if (sched.currentPID == -1)
        snprintf(runBuf, sizeof(runBuf), "[idle]");
    else
        snprintf(runBuf, sizeof(runBuf), "P%d", sched.currentPID);

    printf("\n╔══════════════ SCHEDULER STATE  clock=%-3d ══════════════╗\n", clock);
    printf("  Running : %s\n", runBuf);
    if (sched.algo == SCHED_MLFQ) {
        for (int i = 0; i < MLFQ_LEVELS; i++) {
            char label[32];
            snprintf(label, sizeof(label), "MLFQ Q%d (q=%d):", i, mlfq_quantum(i));
            q_print(&sched.mlfqQ[i], label);
        }
    } else {
        q_print(&sched.readyQ,   "Ready Queue      :");
    }
    q_print(&sched.blockedQ, "Blocked Queue    :");
    printf("╚═══════════════════════════════════════════════════════╝\n");
}

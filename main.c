#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "queue.h"
#include "memory.h"
#include "process.h"
#include "mutex.h"
#include "interpreter.h"
#include "scheduler.h"

/* ─── Arrival Initial configuration  */
#define MAX_PROG 3

static const char *programFiles[MAX_PROG] = {
    "program1.txt",
    "program2.txt",
    "program3.txt"
};
static int arrivalTimes[MAX_PROG] = { 0, 1, 4 };
static int processPIDs [MAX_PROG] = { -1, -1, -1 };

/* ─── Generate the three program text files ──────────────────────── */
static void create_program_files(void) {
    /* Program 1 */
    FILE *f = fopen("program1.txt", "w");
    fprintf(f,
        "semWait userInput\n"
        "assign x input\n"
        "assign y input\n"
        "semSignal userInput\n"
        "semWait userOutput\n"
        "printFromTo x y\n"
        "semSignal userOutput\n");
    fclose(f);

    /* Program 2 */
    f = fopen("program2.txt", "w");
    fprintf(f,
        "semWait userInput\n"
        "assign a input\n"
        "assign b input\n"
        "semSignal userInput\n"
        "semWait file\n"
        "writeFile a b\n"
        "semSignal file\n");
    fclose(f);

    /* Program 3 */
    f = fopen("program3.txt", "w");
    fprintf(f,
        "semWait userInput\n"
        "assign a input\n"
        "semSignal userInput\n"
        "semWait file\n"
        "assign b readFile a\n"
        "semSignal file\n"
        "semWait userOutput\n"
        "print b\n"
        "semSignal userOutput\n");
    fclose(f);

    printf("[INIT] Program files created: program1.txt, program2.txt, program3.txt\n");
}

/*  Check if all processes are done */
static int all_finished(void) {
    /* All programs must have been admitted first */
    for (int i = 0; i < MAX_PROG; i++)
        if (processPIDs[i] == -1) return 0;
    int found = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!proctab[i].active) continue;
        found = 1;
        if (proctab[i].state != FINISHED) return 0;
    }
    return found;
}

/* Print separator*/
static void print_separator(int clock) {
    printf("\n════════════════════════════════════════════════"
           "════════════════════\n");
    printf("  CLOCK CYCLE: %d\n", clock);
    printf("════════════════════════════════════════════════"
           "════════════════════\n");
}

/* Step mode flag */
static int stepMode = 0;

static void step_pause(void) {
    if (!stepMode) return;
    printf("\n  [STEP] Press Enter to advance, 'r' + Enter to run freely: ");
    fflush(stdout);
    char buf[8];
    if (fgets(buf, sizeof(buf), stdin) && (buf[0]=='r'||buf[0]=='R'))
        stepMode = 0;
}

/*Select scheduling algorithm */
static SchedAlgo choose_algo(void) {
    printf("\nSelect scheduling algorithm:\n");
    printf("  1) Round Robin (RR)  – quantum = %d instructions\n", DEFAULT_QUANTUM);
    printf("  2) Highest Response Ratio Next (HRRN)\n");
    printf("  3) Multi-Level Feedback Queue (MLFQ)\n");
    printf("Choice [1-3]: ");
    fflush(stdout);

    int choice = 1;
    char buf[32];
    if (fgets(buf, sizeof(buf), stdin))
        choice = atoi(buf);

    switch (choice) {
        case 2: printf("  → HRRN selected\n");  return SCHED_HRRN;
        case 3: printf("  → MLFQ selected\n");  return SCHED_MLFQ;
        default:printf("  → RR selected\n");    return SCHED_RR;
    }
}

/*Optionally override arrival times */
static void configure_arrivals(void) {
    printf("\nUse default arrival times (P1=0, P2=1, P3=4)? [Y/n]: ");
    fflush(stdout);
    char buf[16];
    if (fgets(buf, sizeof(buf), stdin) && (buf[0]=='n'||buf[0]=='N')) {
        for (int i = 0; i < MAX_PROG; i++) {
            printf("  Arrival time for P%d: ", i + 1);
            fflush(stdout);
            if (fgets(buf, sizeof(buf), stdin))
                arrivalTimes[i] = atoi(buf);
        }
    }
}

/* ─── Main ───────────────────────────────────────────────────────── */
int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║          OS Simulator – GUC CSEN 602             ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* Initialise subsystems */
    mem_init();
    proc_init();
    mutex_init();

    /* Create program .txt files */
    create_program_files();

    /* User configuration */
    SchedAlgo algo = choose_algo();
    configure_arrivals();

    printf("\nRun mode – step through each clock cycle? [y/N]: ");
    fflush(stdout);
    char modeBuf[8];
    if (fgets(modeBuf, sizeof(modeBuf), stdin) && (modeBuf[0]=='y'||modeBuf[0]=='Y'))
        stepMode = 1;

    /* Initialise scheduler */
    sched_init(algo, DEFAULT_QUANTUM);

    printf("\n[SIM] Starting simulation...\n\n");

    int clock          = 0;
    int maxArrivedPIDs = 0;

    /* Run until all processes finish (or we hit a safety limit) */
    while (1) {
        print_separator(clock);

        /* Handle arrivals */
        for (int i = 0; i < MAX_PROG; i++) {
            if (arrivalTimes[i] == clock && processPIDs[i] == -1) {
                printf("\n[ARRIVAL] P%d arrives at clock=%d from '%s'\n",
                       i + 1, clock, programFiles[i]);
                int pid = proc_create(programFiles[i], clock);
                if (pid != -1) {
                    processPIDs[i] = pid;
                    sched_admit(pid, clock);
                    maxArrivedPIDs++;
                    sched_print_queues(clock);
                    proc_print_table();
                }
            }
        }

        /* Early exit if everything is done */
        if (all_finished()) {
            printf("\n[SIM] All processes finished at clock=%d.\n", clock);
            break;
        }

        /*  If no current running process, schedule one */
        if (sched.currentPID == -1) {
            if (!q_empty(&sched.readyQ) ||
                (algo == SCHED_MLFQ && mlfq_pick() != -1)) {
                sched_pick(clock);
                sched_print_queues(clock);
            }
        }

        /* Check if there's anything to do */
        if (sched.currentPID == -1 && q_empty(&sched.readyQ)) {
            /* Check for processes blocked forever (deadlock guard) */
            int blocked_only = 1;
            for (int i = 0; i < MAX_PROCESSES; i++)
                if (proctab[i].active && proctab[i].state == READY)
                { blocked_only = 0; break; }
            if (blocked_only && !q_empty(&sched.blockedQ))
                printf("  [WARN] Only blocked processes remain — possible deadlock!\n");
        }

        /*  Execute one instruction of the current process */
        if (sched.currentPID != -1) {
            int pid    = sched.currentPID;
            int result = interp_exec(pid, &sched.readyQ, &sched.blockedQ, clock);

            if (result == 1) {
                /* FINISHED — free its memory block */
                ProcessEntry *pp = NULL;
                for (int i = 0; i < MAX_PROCESSES; i++)
                    if (proctab[i].active && proctab[i].pid == pid)
                    { pp = &proctab[i]; break; }
                if (pp && !pp->onDisk) {
                    mem_free(pp->lowerBound, pp->upperBound);
                    pp->lowerBound = -1;
                    pp->upperBound = -1;
                }

                printf("\n  [FINISH] P%d memory freed, removed from scheduling.\n", pid);
                q_remove(&sched.readyQ, pid);
                sched.currentPID  = -1;
                sched.quantumLeft = 0;
                sched_print_queues(clock);
                proc_print_table();

            } else if (result == 2) {
                /* BLOCKED */
                sched.currentPID  = -1;
                sched.quantumLeft = 0;
                sched_print_queues(clock);
                proc_print_table();

            } else if (result == 3) {
                /* Swap-in failed temporarily; process re-admitted to ready queue */
                sched.currentPID  = -1;
                sched.quantumLeft = 0;

            } else {
                /* still running: check quantum for RR/MLFQ */
                if (algo != SCHED_HRRN) {
                    sched_tick(clock);
                    if (sched.currentPID == -1) {
                        /* quantum expired */
                        sched_print_queues(clock);
                    }
                }
            }
        } else {
            printf("  [CPU] Idle — no process to run.\n");
        }

        /* ── 5. Print memory and mutex state every clock cycle ──── */
        mem_print(clock);
        mutex_print();

        step_pause();
        clock++;

        /* Safety: prevent infinite loop (e.g., all blocked forever) */
        if (clock > 1000) {
            printf("\n[SIM] Clock limit reached (possible deadlock).\n");
            break;
        }
    }

    /* ── Final summary ──────────────────────────────────────────── */
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║              SIMULATION COMPLETE                 ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("  Total clock cycles: %d\n\n", clock);
    proc_print_table();

    return 0;
}

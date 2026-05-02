# Operating System Simulator

A fully simulated OS kernel built in **C**, modeling real operating system behavior including process scheduling, memory management, disk swapping, and mutex-based resource synchronization — with terminal output visualizing system state per clock cycle.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                        OS Simulator                         │
├───────────────┬─────────────────────┬───────────────────────┤
│   Interpreter │   Process Manager   │   Memory Manager      │
│               │                     │                       │
│  Parses and   │  Lifecycle control  │  40-word fixed alloc  │
│  executes     │  for all processes  │  + disk swap when     │
│  instructions │  (new→ready→run→    │  capacity exceeded    │
│               │   blocked→term)     │                       │
├───────────────┴─────────────────────┴───────────────────────┤
│                      Scheduler                              │
│         RR  |  HRRN  |  MLFQ (4 priority queues)            │
├─────────────────────────────────────────────────────────────┤
│                    Mutex Manager                            │
│     userInput  |  userOutput  |  file  → blocked queue      │
└─────────────────────────────────────────────────────────────┘
```

---

## Features

### Process Lifecycle
Processes move through a full state machine managed per clock cycle:

```
NEW ──► READY ──► RUNNING ──► TERMINATED
                    │
                    ▼
                 BLOCKED (waiting for mutex)
                    │
                    ▼
                  READY (mutex released)
```

### Scheduling Algorithms

| Algorithm | Description | Use Case |
|---|---|---|
| **Round Robin (RR)** | Fixed time quantum, cyclic execution | Fair CPU sharing |
| **HRRN** | Highest Response Ratio Next — prioritizes waiting time | Prevents starvation |
| **MLFQ** | 4-level priority queue, demotes on long bursts | General purpose OS |

### Memory Management

| Property | Value |
|---|---|
| Allocation per process | 40 words (fixed) |
| Overflow strategy | Disk swapping |
| Visualization | Per clock cycle in terminal |

### Mutex & Synchronization

Three shared resources protected by mutex locks:

| Resource | Type |
|---|---|
| `userInput` | Mutual exclusion |
| `userOutput` | Mutual exclusion |
| `file` | Mutual exclusion |

Processes blocked on a locked resource are moved to a **blocked queue** and resumed automatically on release.

---

## File Structure

| File | Responsibility |
|---|---|
| `main.c` | Entry point, clock cycle loop |
| `process.c/h` | Process struct, lifecycle transitions |
| `scheduler.c/h` | RR, HRRN, MLFQ implementations |
| `memory.c/h` | Memory allocation and disk swap logic |
| `mutex.c/h` | Mutex locks and blocked queue management |
| `queue.c/h` | Generic queue data structure |
| `interpreter.c/h` | Instruction parsing and execution |
| `types.h` | Shared type definitions |
| `Makefile` | Build configuration |

---

## Build & Run

```bash
make
./os_simulator
```

### Sample Terminal Output
```
Clock Cycle 4:
──────────────────────────────
Running:  P2 (instruction 3/7)
Ready:    [P1, P3]
Blocked:  [P4 → waiting on file]
Memory:   [P1: 0-39] [P2: 40-79] [P3: 80-119] [P4: DISK]
──────────────────────────────
```

---

## Tech Stack

![C](https://img.shields.io/badge/Language-C-blue)
![Makefile](https://img.shields.io/badge/Build-Makefile-lightgrey)
![OS](https://img.shields.io/badge/Domain-Operating%20Systems-green)

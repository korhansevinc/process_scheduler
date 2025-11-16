# Priority-SRTF Process Scheduler

A sophisticated process scheduler implementation in C that combines **Priority scheduling** with **Shortest Remaining Time First (SRTF)** using a non-preemptive approach. This project demonstrates advanced operating system concepts including process management, threading, synchronization, and aging mechanisms.

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Technical Architecture](#technical-architecture)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Input File Format](#input-file-format)
- [Output Format](#output-format)
- [Scheduling Algorithm](#scheduling-algorithm)
- [Implementation Details](#implementation-details)
- [Example Execution](#example-execution)
- [Project Structure](#project-structure)
- [Performance Considerations](#performance-considerations)
- [Contributing](#contributing)
- [License](#license)

## 🎯 Overview

This process scheduler simulates a CPU scheduler that manages process execution using a hybrid Priority-SRTF (Shortest Remaining Time First) algorithm. The scheduler is **non-preemptive**, meaning once a process begins execution, it runs for its entire burst time without interruption.

### Key Concepts

- **Priority Scheduling**: Processes are selected based on priority (0 = highest, 10 = lowest)
- **SRTF Tie-Breaking**: When multiple processes have the same priority, the one with the shortest remaining CPU time is selected
- **Non-Preemptive Execution**: Running processes complete their current burst before yielding the CPU
- **Aging Mechanism**: Prevents starvation by incrementing process priority over time
- **I/O Management**: Separate thread handles I/O operations asynchronously

## ✨ Features

### Core Functionality

- ✅ **Priority-based scheduling** with configurable priority levels (0-10)
- ✅ **SRTF secondary criterion** for efficient process selection
- ✅ **Aging mechanism** that decrements priority every 100ms to prevent starvation
- ✅ **Multi-threaded architecture** with dedicated I/O manager thread
- ✅ **Thread-safe operations** using mutexes for queue and output synchronization
- ✅ **Dynamic process arrival** handling at runtime
- ✅ **I/O operation simulation** with blocking and resumption
- ✅ **Comprehensive error checking** on all system calls

### Scheduler Characteristics

- **Type**: Non-preemptive
- **Primary Criterion**: Priority (0 = highest priority)
- **Secondary Criterion**: SRTF (Shortest Remaining Time First)
- **Aging**: Priority decremented by 1 every 100ms in ready queue
- **Threads**: Main scheduler thread + dedicated I/O manager thread
- **Queues**: Priority-sorted ready queue + FIFO waiting queue

## 🏗️ Technical Architecture

### Threading Model

```
┌─────────────────────────────────────────────────────────────┐
│                    Main Scheduler Thread                     │
│  • Process arrivals                                          │
│  • Time management (1ms clock ticks)                         │
│  • Priority-SRTF selection                                   │
│  • CPU dispatching                                           │
│  • Aging mechanism updates                                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ Synchronization (mutexes)
                              │
┌─────────────────────────────────────────────────────────────┐
│                    I/O Manager Thread                        │
│  • Monitors waiting queue                                    │
│  • Detects I/O completion                                    │
│  • Moves processes back to ready queue                       │
└─────────────────────────────────────────────────────────────┘
```

### Process State Diagram

```
    NEW → READY ──┐
            ↑     │
            │     ↓
       WAITING ← RUNNING → TERMINATED
```

### Data Structures

- **Process Control Block (PCB)**: Contains all process metadata
- **Ready Queue**: Priority-sorted queue (Priority-SRTF order)
- **Waiting Queue**: FIFO queue for I/O-blocked processes
- **Mutexes**: Ensure thread-safe access to shared resources

## 📦 Requirements

### System Requirements

- **Operating System**: Linux/Unix-based system
- **Compiler**: GCC with C99 support
- **Libraries**: 
  - `pthread` (POSIX threads)
  - `stdio`, `stdlib`, `string` (Standard C library)
  - `unistd`, `limits`, `sys/time` (POSIX utilities)

### Build Requirements

- GNU Make
- GCC compiler with pthread support

## 🔧 Installation

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/priority-srtf-scheduler.git
cd priority-srtf-scheduler
```

### 2. Compile the Project

```bash
make
```

This will generate the `process_scheduler` executable.

### 3. Clean Build (Optional)

```bash
make clean    # Remove compiled binaries
make          # Rebuild from scratch
```

## 🚀 Usage

### Basic Usage

```bash
./process_scheduler <input_file>
```

### Example

```bash
./process_scheduler processes.txt
```

### Command-Line Arguments

| Argument | Description | Required |
|----------|-------------|----------|
| `input_file` | Path to the input file containing process definitions | Yes |

## 📄 Input File Format

Each line in the input file represents one process with the following format:

```
[process_id] [arrival_time] [cpu_execution_time] [interval_time] [io_time] [priority]
```

### Field Descriptions

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `process_id` | Integer | Unique identifier for the process | `1` |
| `arrival_time` | Integer (ms) | Time when process arrives at ready queue | `10` |
| `cpu_execution_time` | Integer (ms) | Total CPU time needed by the process | `100` |
| `interval_time` | Integer (ms) | Duration of each CPU burst before I/O | `25` |
| `io_time` | Integer (ms) | Duration of each I/O operation | `5` |
| `priority` | Integer (0-10) | Process priority (0 = highest) | `2` |

### Example Input File

```text
1 0 100 25 5 2
2 5 30 10 3 1
3 10 60 30 8 2
```

#### Process Explanation

**Process 1** (`1 0 100 25 5 2`):
- Arrives at time 0ms
- Needs 100ms total CPU time
- Runs in 25ms bursts (4 bursts total)
- 5ms I/O after each burst (except last)
- Priority level 2

**Process 2** (`2 5 30 10 3 1`):
- Arrives at time 5ms
- Needs 30ms total CPU time
- Runs in 10ms bursts (3 bursts total)
- 3ms I/O after each burst (except last)
- Priority level 1 (higher priority than processes 1 & 3)

**Process 3** (`3 10 60 30 8 2`):
- Arrives at time 10ms
- Needs 60ms total CPU time
- Runs in 30ms bursts (2 bursts total)
- 8ms I/O after each burst (except last)
- Priority level 2

## 📊 Output Format

The scheduler produces timestamped output for all significant events:

### Output Messages

| Event | Format |
|-------|--------|
| Process Arrival | `[Clock: $clock] PID $pid arrived` |
| Ready Queue Entry | `[Clock: $clock] PID $pid moved to READY queue` |
| CPU Dispatch | `[Clock: $clock] Scheduler dispatched PID $pid (Pr: $priority, Rm: $remaining_time) for $interval_time ms burst` |
| I/O Blocking | `[Clock: $clock] PID $pid blocked for I/O for $io_time ms` |
| I/O Completion | `[Clock: $clock] PID $pid finished I/O` |
| Termination | `[Clock: $clock] PID $pid TERMINATED` |

### Example Output

```
[Clock: 1] PID 1 arrived
[Clock: 1] PID 1 moved to READY queue
[Clock: 1] Scheduler dispatched PID 1 (Pr: 2, Rm: 100) for 25 ms burst
[Clock: 5] PID 2 arrived
[Clock: 5] PID 2 moved to READY queue
[Clock: 26] PID 1 blocked for I/O for 5 ms
[Clock: 26] Scheduler dispatched PID 2 (Pr: 1, Rm: 30) for 10 ms burst
[Clock: 31] PID 1 finished I/O
[Clock: 31] PID 1 moved to READY queue
...
```

## 🧮 Scheduling Algorithm

### Selection Criteria (Priority-SRTF)

The scheduler selects processes from the ready queue based on:

1. **Primary**: Priority level (lower number = higher priority)
2. **Secondary**: Remaining CPU time (shorter time = higher priority)

### Algorithm Pseudocode

```
function select_next_process():
    for each process in ready_queue:
        if process.priority < selected.priority:
            selected = process
        else if process.priority == selected.priority:
            if process.remaining_time < selected.remaining_time:
                selected = process
    return selected
```

### Aging Mechanism

To prevent starvation, the scheduler implements an aging mechanism:

- **Trigger**: Every 100ms a process spends in the ready queue
- **Action**: Priority is decremented by 1 (moves toward higher priority)
- **Limit**: Priority cannot go below 0
- **Reset**: Timer resets when process is dispatched

```
Time in Ready Queue    Priority Change
0-99 ms               No change
100-199 ms            -1
200-299 ms            -2
...                   ...
```

### Non-Preemptive Behavior

Once a process begins executing:
- It runs for its **full interval_time** or until completion
- **No interruption** occurs even if higher-priority processes arrive
- After the burst, the process either:
  - Goes to **waiting queue** (if I/O needed)
  - **Terminates** (if all CPU time consumed)
  - Continues in next burst

## 🔍 Implementation Details

### Thread Synchronization

Three mutexes ensure thread safety:

```c
pthread_mutex_t queue_mutex;    // Protects ready and waiting queues
pthread_mutex_t clock_mutex;    // Protects global clock
pthread_mutex_t output_mutex;   // Prevents interleaved output
```

### Key Functions

| Function | Purpose |
|----------|---------|
| `parse_input_file()` | Reads and validates input file |
| `insert_ready_queue()` | Inserts process maintaining Priority-SRTF order |
| `run_scheduler()` | Main scheduling loop (arrivals, dispatching, aging) |
| `io_manager_thread()` | I/O thread that manages waiting queue |
| `update_aging()` | Implements aging mechanism |
| `resort_ready_queue()` | Re-sorts ready queue after priority changes |

### Time Management

- **Clock Granularity**: 1 millisecond
- **Implementation**: `usleep(1000)` for 1ms sleep
- **Synchronization**: Global clock protected by mutex

### Error Handling

All system calls include error checking:

```c
if (pthread_create(&io_thread, NULL, io_manager_thread, NULL) != 0) {
    perror("Error creating I/O manager thread");
    // Cleanup and exit
}
```

## 📸 Example Execution

### Test Scenario

Input file (`processes.txt`):
```
1 0 100 25 5 2
2 5 30 10 3 1
3 10 60 30 8 2
```

### Execution Timeline

```
Time 0-1:    Process 1 arrives (Priority 2)
Time 1-26:   Process 1 runs (1st burst, 25ms)
Time 5:      Process 2 arrives (Priority 1 - higher!)
Time 10:     Process 3 arrives (Priority 2)
Time 26-31:  Process 1 in I/O, Process 2 runs (higher priority)
Time 31:     Process 1 returns to ready queue
Time 36-66:  Process 2 in I/O, Process 3 runs
...          (continues until all processes terminate)
```

### Key Observations

1. **Process 2 preempts waiting** due to higher priority (1 vs 2)
2. **Aging not visible** in this short example (no process waits 100ms)
3. **Non-preemptive** behavior: each burst completes fully
4. **I/O operations** are managed by separate thread

## 📁 Project Structure

```
priority-srtf-scheduler/
├── README.md                    # This file
├── Makefile                     # Build configuration
├── process_scheduler.c          # Main implementation (650 lines)
├── processes.txt               # Example input file
└── Operating Systems Homework 2.pdf  # Assignment specification
```

### Code Organization (process_scheduler.c)

```
Lines   Section
------  --------------------------------------------------
1-40    Header comments and includes
41-75   Data structures (Process, Queue, States)
76-90   Global variables and mutexes
91-180  Queue operations (enqueue, dequeue, insert, remove)
181-250 Input file parsing
251-270 Thread-safe output functions
271-345 I/O manager thread
346-420 Aging mechanism
421-590 Main scheduler logic
591-649 Main function and initialization
```

## ⚡ Performance Considerations

### Time Complexity

- **Process Selection**: O(1) - ready queue is pre-sorted
- **Queue Insertion**: O(n) - maintain sorted order
- **Aging Update**: O(n) - update all ready queue processes
- **I/O Check**: O(m) - check all waiting processes

Where:
- n = number of processes in ready queue
- m = number of processes in waiting queue

### Space Complexity

- **Process Storage**: O(N) - N total processes
- **Queue Storage**: O(N) - at most N processes in queues
- **Overall**: O(N) linear space

### Optimization Techniques

1. **Pre-sorted Queue**: Ready queue maintained in Priority-SRTF order
2. **Minimal Locking**: Mutexes held for shortest possible duration
3. **Efficient I/O Checking**: O(1) check per waiting process
4. **Aging Batching**: Updates processed every 1ms, not per microsecond

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/improvement`)
3. Commit your changes (`git commit -am 'Add new feature'`)
4. Push to the branch (`git push origin feature/improvement`)
5. Create a Pull Request

### Code Style

- Follow Linux kernel coding style
- Use descriptive variable names
- Add comments for complex logic
- Ensure all code compiles without warnings

## 📝 License

This project is developed as part of an Operating Systems course assignment. Feel free to use it for educational purposes.

---

## Author

**Korhan Sevinç**
- Operating Systems Course Project
- Year: 2025

---

**⭐ If you found this project helpful, please consider giving it a star!**


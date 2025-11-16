/*
 * process_scheduler.c
 * 
 * Priority-SRTF Based Non-Preemptive Process Scheduler
 * 
 * This program implements a process scheduler that uses Priority scheduling
 * as the primary criterion and Shortest Remaining Time First (SRTF) as the
 * secondary criterion when priorities are equal. The scheduler is non-preemptive,
 * meaning once a process starts running, it runs for its full interval_time burst.
 * 
 * Features:
 * - Priority-based scheduling (0 = highest, 10 = lowest)
 * - SRTF for tie-breaking when priorities are equal
 * - Aging mechanism: priority decrements by 1 every 100ms in ready queue
 * - I/O management via separate pthread
 * - Non-preemptive execution
 * 
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <sys/time.h>
#include <stdarg.h>

/* Explicit declaration for usleep to avoid warnings with -std=gnu99 */
int usleep(unsigned int usec);

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * Process State Enumeration
 * Represents the current state of a process in the system
 */
typedef enum {
    STATE_NEW,          // Process has arrived but not yet in ready queue
    STATE_READY,        // Process is in ready queue waiting for CPU
    STATE_RUNNING,      // Process is currently executing on CPU
    STATE_WAITING,      // Process is blocked waiting for I/O
    STATE_TERMINATED    // Process has completed execution
} ProcessState;

/**
 * Process Control Block (PCB)
 * Contains all information about a process
 */
typedef struct Process {
    int pid;                        // Process ID
    int arrival_time;               // Time when process arrives (ms)
    int cpu_execution_time;         // Total CPU time needed (ms)
    int remaining_time;             // Remaining CPU time (ms)
    int interval_time;              // Time for each CPU burst (ms)
    int io_time;                    // Time for each I/O operation (ms)
    int priority;                   // Current priority (0=highest, 10=lowest)
    int original_priority;          // Original priority (for reference)
    
    ProcessState state;             // Current state
    int time_in_ready_queue;        // Time spent in ready queue (for aging)
    int io_completion_time;         // When current I/O will complete
    int has_arrived;                // Flag: has process arrived yet?
    
    struct Process *next;           // Pointer to next process in queue
} Process;

/**
 * Queue Structure
 * Generic queue for ready and waiting processes
 */
typedef struct {
    Process *head;
    Process *tail;
    int size;
} Queue;

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */

Process *all_processes = NULL;      // Array of all processes
int total_processes = 0;             // Total number of processes
int current_clock = 0;               // Global clock (ms)
int all_terminated = 0;              // Flag: all processes terminated?

Queue ready_queue;                   // Ready queue
Queue waiting_queue;                 // Waiting queue (I/O)

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================================
 * QUEUE OPERATIONS
 * ============================================================================ */

/**
 * Initialize a queue
 */
void init_queue(Queue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
}

/**
 * Check if queue is empty
 */
int is_empty(Queue *q) {
    return q->size == 0;
}

/**
 * Enqueue a process to the end of queue
 * This is used for waiting queue (FIFO order for I/O)
 */
void enqueue(Queue *q, Process *p) {
    p->next = NULL;
    if (q->tail == NULL) {
        q->head = p;
        q->tail = p;
    } else {
        q->tail->next = p;
        q->tail = p;
    }
    q->size++;
}

/**
 * Dequeue a process from the front of queue
 */
Process* dequeue(Queue *q) {
    if (is_empty(q)) {
        return NULL;
    }
    
    Process *p = q->head;
    q->head = q->head->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    q->size--;
    p->next = NULL;
    return p;
}

/**
 * Insert process into ready queue based on Priority-SRTF
 * Primary: Lower priority number = higher priority (0 is highest)
 * Secondary: Lower remaining_time = higher priority (SRTF)
 */
void insert_ready_queue(Queue *q, Process *p) {
    p->next = NULL;
    p->time_in_ready_queue = 0;  // Reset aging timer when entering ready queue
    
    // Empty queue case
    if (q->head == NULL) {
        q->head = p;
        q->tail = p;
        q->size++;
        return;
    }
    
    // Find correct position based on Priority-SRTF
    Process *current = q->head;
    Process *prev = NULL;
    
    while (current != NULL) {
        // Compare: first by priority, then by remaining_time
        int should_insert = 0;
        if (p->priority < current->priority) {
            should_insert = 1;  // Higher priority (lower number)
        } else if (p->priority == current->priority && 
                   p->remaining_time < current->remaining_time) {
            should_insert = 1;  // Same priority, shorter remaining time
        }
        
        if (should_insert) {
            break;
        }
        
        prev = current;
        current = current->next;
    }
    
    // Insert at found position
    if (prev == NULL) {
        // Insert at head
        p->next = q->head;
        q->head = p;
    } else {
        // Insert in middle or end
        p->next = current;
        prev->next = p;
        if (current == NULL) {
            q->tail = p;  // Inserted at end
        }
    }
    q->size++;
}

/**
 * Remove a specific process from queue
 * Used when a process needs to be removed from middle of queue
 */
void remove_from_queue(Queue *q, Process *p) {
    if (is_empty(q)) {
        return;
    }
    
    Process *current = q->head;
    Process *prev = NULL;
    
    while (current != NULL) {
        if (current == p) {
            if (prev == NULL) {
                // Remove from head
                q->head = current->next;
                if (q->head == NULL) {
                    q->tail = NULL;
                }
            } else {
                prev->next = current->next;
                if (current->next == NULL) {
                    q->tail = prev;
                }
            }
            q->size--;
            current->next = NULL;
            return;
        }
        prev = current;
        current = current->next;
    }
}

/* ============================================================================
 * INPUT PARSING
 * ============================================================================ */

/**
 * Parse input file and load all processes
 * File format: [pid] [arrival_time] [cpu_execution_time] [interval_time] [io_time] [priority]
 */
int parse_input_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening input file");
        return -1;
    }
    
    // Count number of processes
    total_processes = 0;
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strlen(line) > 1) {  // Skip empty lines
            total_processes++;
        }
    }
    
    if (total_processes == 0) {
        fprintf(stderr, "Error: No processes found in input file\n");
        fclose(file);
        return -1;
    }
    
    // Allocate memory for processes
    all_processes = (Process *)malloc(sizeof(Process) * total_processes);
    if (all_processes == NULL) {
        perror("Error allocating memory for processes");
        fclose(file);
        return -1;
    }
    
    // Read processes
    rewind(file);
    int idx = 0;
    while (fgets(line, sizeof(line), file) != NULL && idx < total_processes) {
        if (strlen(line) <= 1) continue;  // Skip empty lines
        
        int pid, arrival, cpu_time, interval, io, priority;
        if (sscanf(line, "%d %d %d %d %d %d", 
                   &pid, &arrival, &cpu_time, &interval, &io, &priority) != 6) {
            fprintf(stderr, "Error: Invalid format in input file at line %d\n", idx + 1);
            free(all_processes);
            fclose(file);
            return -1;
        }
        
        // Initialize process
        all_processes[idx].pid = pid;
        all_processes[idx].arrival_time = arrival;
        all_processes[idx].cpu_execution_time = cpu_time;
        all_processes[idx].remaining_time = cpu_time;
        all_processes[idx].interval_time = interval;
        all_processes[idx].io_time = io;
        all_processes[idx].priority = priority;
        all_processes[idx].original_priority = priority;
        all_processes[idx].state = STATE_NEW;
        all_processes[idx].time_in_ready_queue = 0;
        all_processes[idx].io_completion_time = 0;
        all_processes[idx].has_arrived = 0;
        all_processes[idx].next = NULL;
        
        idx++;
    }
    
    fclose(file);
    return 0;
}

/* ============================================================================
 * OUTPUT FUNCTIONS
 * ============================================================================ */

/**
 * Thread-safe print function
 * Ensures atomic output to prevent interleaved messages
 */
void safe_print(const char *format, ...) {
    pthread_mutex_lock(&output_mutex);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    pthread_mutex_unlock(&output_mutex);
}

/* ============================================================================
 * I/O MANAGER THREAD
 * ============================================================================ */

/**
 * I/O Manager Thread Function
 * 
 * Manages processes in the waiting queue. Checks each millisecond if any
 * process has completed its I/O operation and moves it back to ready queue.
 */
void* io_manager_thread(void *arg) {
    (void)arg;  // Unused parameter
    
    while (!all_terminated) {
        usleep(1000);  // Sleep for 1ms
        
        pthread_mutex_lock(&clock_mutex);
        int clock = current_clock;
        pthread_mutex_unlock(&clock_mutex);
        
        pthread_mutex_lock(&queue_mutex);
        
        // Check all processes in waiting queue
        Process *current = waiting_queue.head;
        Process *prev = NULL;
        
        while (current != NULL) {
            if (clock >= current->io_completion_time) {
                // I/O completed
                Process *completed = current;
                current = current->next;
                
                // Remove from waiting queue
                if (prev == NULL) {
                    waiting_queue.head = completed->next;
                    if (waiting_queue.head == NULL) {
                        waiting_queue.tail = NULL;
                    }
                } else {
                    prev->next = completed->next;
                    if (completed->next == NULL) {
                        waiting_queue.tail = prev;
                    }
                }
                waiting_queue.size--;
                
                completed->next = NULL;
                completed->state = STATE_READY;
                
                // Output: I/O finished
                pthread_mutex_lock(&output_mutex);
                printf("[Clock: %d] PID %d finished I/O\n", clock, completed->pid);
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);
                
                // Move to ready queue
                insert_ready_queue(&ready_queue, completed);
                
                pthread_mutex_lock(&output_mutex);
                printf("[Clock: %d] PID %d moved to READY queue\n", clock, completed->pid);
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);
                
            } else {
                prev = current;
                current = current->next;
            }
        }
        
        pthread_mutex_unlock(&queue_mutex);
    }
    
    return NULL;
}

/* ============================================================================
 * AGING MECHANISM
 * ============================================================================ */

/**
 * Update aging for all processes in ready queue
 * Decrement priority by 1 for every 100ms spent in ready queue
 * Priority cannot go below 0
 */
void update_aging(int elapsed_ms) {
    Process *current = ready_queue.head;
    
    while (current != NULL) {
        current->time_in_ready_queue += elapsed_ms;
        
        // Check if 100ms threshold reached
        if (current->time_in_ready_queue >= 100) {
            int aging_steps = current->time_in_ready_queue / 100;
            current->time_in_ready_queue %= 100;  // Keep remainder
            
            // Decrement priority (but not below 0)
            if (current->priority > 0) {
                current->priority -= aging_steps;
                if (current->priority < 0) {
                    current->priority = 0;
                }
            }
        }
        
        current = current->next;
    }
}

/**
 * Re-sort ready queue after aging updates
 * Remove all processes and re-insert them based on new priorities
 */
void resort_ready_queue() {
    if (ready_queue.size <= 1) {
        return;  // No need to sort
    }
    
    // Collect all processes
    Process *processes[ready_queue.size];
    int count = 0;
    
    while (!is_empty(&ready_queue)) {
        processes[count++] = dequeue(&ready_queue);
    }
    
    // Re-insert with new priorities
    for (int i = 0; i < count; i++) {
        insert_ready_queue(&ready_queue, processes[i]);
    }
}

/* ============================================================================
 * SCHEDULER
 * ============================================================================ */

/**
 * Main Scheduler Function
 * 
 * Implements Priority-SRTF non-preemptive scheduling:
 * 1. Check for arriving processes
 * 2. Update aging mechanism
 * 3. Select next process from ready queue (already sorted by Priority-SRTF)
 * 4. Run process for its interval_time (non-preemptive)
 * 5. Handle I/O or termination
 */
void run_scheduler() {
    Process *running_process = NULL;
    int running_until = 0;  // When current process will finish its burst
    int last_aging_check = 0;  // Last time we checked aging
    
    while (1) {
        pthread_mutex_lock(&clock_mutex);
        current_clock++;
        int clock = current_clock;
        pthread_mutex_unlock(&clock_mutex);
        
        pthread_mutex_lock(&queue_mutex);
        
        // Check for new arrivals
        for (int i = 0; i < total_processes; i++) {
            if (!all_processes[i].has_arrived && 
                all_processes[i].arrival_time <= clock) {
                
                all_processes[i].has_arrived = 1;
                
                pthread_mutex_lock(&output_mutex);
                printf("[Clock: %d] PID %d arrived\n", clock, all_processes[i].pid);
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);
                
                all_processes[i].state = STATE_READY;
                insert_ready_queue(&ready_queue, &all_processes[i]);
                
                pthread_mutex_lock(&output_mutex);
                printf("[Clock: %d] PID %d moved to READY queue\n", 
                       clock, all_processes[i].pid);
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);
            }
        }
        
        // Update aging every 1ms for processes in ready queue
        if (clock > last_aging_check) {
            int elapsed = clock - last_aging_check;
            update_aging(elapsed);
            resort_ready_queue();
            last_aging_check = clock;
        }
        
        // Check if current running process has finished its burst
        if (running_process != NULL && clock >= running_until) {
            // Process finished its interval burst
            int burst_time = clock - (running_until - running_process->interval_time);
            if (burst_time > running_process->remaining_time) {
                burst_time = running_process->remaining_time;
            }
            
            running_process->remaining_time -= burst_time;
            
            if (running_process->remaining_time <= 0) {
                // Process terminated
                running_process->state = STATE_TERMINATED;
                
                pthread_mutex_lock(&output_mutex);
                printf("[Clock: %d] PID %d TERMINATED\n", clock, running_process->pid);
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);
                
                running_process = NULL;
            } else {
                // Process needs I/O
                running_process->state = STATE_WAITING;
                running_process->io_completion_time = clock + running_process->io_time;
                
                pthread_mutex_lock(&output_mutex);
                printf("[Clock: %d] PID %d blocked for I/O for %d ms\n", 
                       clock, running_process->pid, running_process->io_time);
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);
                
                enqueue(&waiting_queue, running_process);
                running_process = NULL;
            }
        }
        
        // Schedule next process if CPU is idle
        if (running_process == NULL && !is_empty(&ready_queue)) {
            running_process = dequeue(&ready_queue);
            running_process->state = STATE_RUNNING;
            
            // Calculate actual burst time (minimum of interval_time and remaining_time)
            int burst_time = running_process->interval_time;
            if (burst_time > running_process->remaining_time) {
                burst_time = running_process->remaining_time;
            }
            
            running_until = clock + burst_time;
            
            pthread_mutex_lock(&output_mutex);
            printf("[Clock: %d] Scheduler dispatched PID %d (Pr: %d, Rm: %d) for %d ms burst\n",
                   clock, running_process->pid, running_process->priority, 
                   running_process->remaining_time, burst_time);
            fflush(stdout);
            pthread_mutex_unlock(&output_mutex);
        }
        
        // Check if all processes are terminated
        int all_done = 1;
        for (int i = 0; i < total_processes; i++) {
            if (all_processes[i].state != STATE_TERMINATED) {
                all_done = 0;
                break;
            }
        }
        
        if (all_done && running_process == NULL) {
            all_terminated = 1;
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        
        pthread_mutex_unlock(&queue_mutex);
        
        usleep(1000);  // Sleep for 1ms
    }
}

/* ============================================================================
 * MAIN FUNCTION
 * ============================================================================ */

int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Parse input file
    if (parse_input_file(argv[1]) != 0) {
        return EXIT_FAILURE;
    }
    
    // Initialize queues
    init_queue(&ready_queue);
    init_queue(&waiting_queue);
    
    // Create I/O manager thread
    pthread_t io_thread;
    if (pthread_create(&io_thread, NULL, io_manager_thread, NULL) != 0) {
        perror("Error creating I/O manager thread");
        free(all_processes);
        return EXIT_FAILURE;
    }
    
    // Run scheduler in main thread
    run_scheduler();
    
    // Wait for I/O thread to finish
    if (pthread_join(io_thread, NULL) != 0) {
        perror("Error joining I/O manager thread");
    }
    
    // Cleanup
    free(all_processes);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&clock_mutex);
    pthread_mutex_destroy(&output_mutex);
    
    return EXIT_SUCCESS;
}


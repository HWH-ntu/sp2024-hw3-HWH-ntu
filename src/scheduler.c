#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "routine.h"
#include "thread_tool.h"

struct tcb *sleeping_set[THREAD_MAX];

// TODO::
// Prints out the signal you received.
// This function should not return. Instead, jumps to the scheduler.
void sighandler(int signum) { // signal handler to catch 'SIGALRM' or 'SIGTSTP'
    if(signum == SIGALRM) {
        printf("caught SIGALRM\n");
    } else if (signum == SIGTSTP) {
        printf("caught SIGTSTP\n");
    }

    // switch to the scheduler context
    longjmp(sched_buf, FROM_sighandler);
}

// TODO::
// Perfectly setting up your scheduler.
void scheduler() {

    // initialize the idel thread
    thread_create(idle, 0, NULL);

    // Save scheduler context in sched_buf
    int setjmpret = setjmp(sched_buf);

    // Step 1: Reset the alarm
    alarm(0); // cancel any previous alarm
    alarm(time_slice); // set a new alarm for the time slice
    
    // Step 2: Clear pending signals
    struct sigaction old_sigalrm, old_sigtstp, ignore_action;
    ignore_action.sa_handler = SIG_IGN; // sig_ignore
    sigemptyset(&ignore_action.sa_mask);
    ignore_action.sa_flags = 0;

    // Temporarily ignore SIGALRM and SIGTSTP.
    if (sigaction(SIGALRM, &ignore_action, &old_sigalrm) == -1) {
        perror("Failed to ignore SIGALRM");
        exit(1);
    }
    if (sigaction(SIGTSTP, &ignore_action, &old_sigtstp) == -1) {
        perror("Failed to ignore SIGTSTP");
        exit(1);
    }

    // Restore the original signal handlers.
    if (sigaction(SIGALRM, &old_sigalrm, NULL) == -1) {
        perror("Failed to restore SIGALRM handler");
        exit(1);
    }
    if (sigaction(SIGTSTP, &old_sigtstp, NULL) == -1) {
        perror("Failed to restore SIGTSTP handler");
        exit(1);
    }

    // Step 3: manage sleeping threads
    for (int i=0; i <THREAD_MAX; i++) {
        struct tcb* thread = sleeping_set[i];
        if(thread) {
            thread->sleeping_time -= time_slice; // decrement the thread's sleep duration

            if(thread->sleeping_time <= 0){
                sleeping_set[i] = NULL;

                ready_queue.arr[ready_queue.tail] = thread;
                ready_queue.tail = (ready_queue.tail +1) % THREAD_MAX;
                ready_queue.size++;

                if (DEBUG) fprintf(stderr, "thread %d: Step 3: manage sleeping threads\n", thread->id);               
            }
        }
    }

    // Step 4: Handle waiting threads
    int iterations = waiting_queue.size; // Avoid infinite loops; only process existing threads
    while (iterations-- > 0 && waiting_queue.size > 0) {
        struct tcb *thread = waiting_queue.arr[waiting_queue.head];

        bool resource_available = false;

        // Check resource availability based on the thread's waiting_for field
        if (thread->waiting_for == 1 && rwlock.write_count == 0) {
            // Read lock requested and no write lock is active
            resource_available = true;
            rwlock.read_count++; // Grant read lock
        } else if (thread->waiting_for == 2 && rwlock.read_count == 0 && rwlock.write_count == 0) {
            // Write lock requested and no other locks are active
            resource_available = true;
            rwlock.write_count++; // Grant write lock
        }

        if (resource_available) {
            // Remove from waiting queue
            waiting_queue.head = (waiting_queue.head + 1) % THREAD_MAX;
            waiting_queue.size--;

            // Add to ready queue
            ready_queue.arr[ready_queue.tail] = thread;
            ready_queue.tail = (ready_queue.tail + 1) % THREAD_MAX;
            ready_queue.size++;

            if (DEBUG) fprintf(stderr, "Thread %d: Step 4: Handle waiting threads\n", thread->id);
        } else {
            // Recycle the thread to the end of the waiting queue
            waiting_queue.head = (waiting_queue.head + 1) % THREAD_MAX;
            waiting_queue.arr[waiting_queue.tail] = thread;
            waiting_queue.tail = (waiting_queue.tail + 1) % THREAD_MAX;
        }
    }

    // **Step 5: Handle previously running threads**
    if (DEBUG) printf("before source. setjmp for thread %d\n", current_thread->id);

    switch (setjmpret) {
        case 0:
            break;
        case FROM_sighandler:
        case FROM_thread_yield:
            if (current_thread && current_thread != idle_thread) {
                ready_queue.arr[ready_queue.tail] = current_thread;
                ready_queue.tail = (ready_queue.tail + 1) % THREAD_MAX;
                ready_queue.size++;
                current_thread = NULL;
                if (DEBUG) fprintf(stderr, "Thread %d: Step 5: Handle previously running threads\n", current_thread->id);
            }
            break;

        case FROM_read_lock:
        case FROM_write_lock:
            waiting_queue.arr[waiting_queue.tail] = current_thread;
            waiting_queue.tail = (waiting_queue.tail + 1) % THREAD_MAX;
            waiting_queue.size++;
            if (DEBUG) fprintf(stderr, "Thread %d: Waiting for resource and moved to waiting queue\n", current_thread->id);
            break;

        case FROM_thread_sleep:
            // Thread already in the sleeping set; no action required
            if (DEBUG) fprintf(stderr, "Thread %d: Sleeping\n", current_thread->id);
            break;

        case FROM_thread_exit:
            // Free resources and do not add back to any queue
            if (DEBUG) fprintf(stderr, "Thread %d: Exited and resources freed\n", current_thread->id);
            free(current_thread->args);
            free(current_thread);
            current_thread = NULL;
            break;

        default:
            //fprintf(stderr, "Unknown jump source: %d\n", source);
            exit(1);
    }

    // Step 6: Select the next thread to run
    if(ready_queue.size > 0) {
        current_thread = ready_queue.arr[ready_queue.head];
        ready_queue.head = (ready_queue.head +1) % THREAD_MAX;
        ready_queue.size--;
        if (DEBUG) fprintf(stderr, "Step 6: Ready to run: %d\n", current_thread->id);
        longjmp(current_thread->env, FROM_scheduler); // context switch to the selected thread
    }

    // If no threads are ready, check sleeping_set and schedule idle thread
    bool any_sleeping = false;
    for (int i = 0; i < THREAD_MAX; i++) {
        if (sleeping_set[i]) {
            any_sleeping = true;
            break;
        }
    }

    if (any_sleeping && idle_thread) {
        current_thread = idle_thread;
        longjmp(idle_thread->env, FROM_scheduler);
    } else {
        if (DEBUG) fprintf(stderr, "Scheduler: No more threads, exiting.\n");
        free(idle_thread);
        idle_thread = NULL;
        return;
    }
    
}

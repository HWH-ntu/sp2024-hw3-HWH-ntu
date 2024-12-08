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
void sighandler(int signum) {
    // Your code here
}

// TODO::
// Perfectly setting up your scheduler.
void scheduler() {
    while(1) {
        for (int i=0; i <THREAD_MAX; i++) {
            if(sleeping_set[i]) {
                sleeping_set[i]->sleeping_time -= time_slice;
                if(sleeping_set[i]->sleeping_time <= 0) {
                    thread_awake(i);
                }
            }
        }
        if(ready_queue.size > 0) {
            current_thread = ready_queue.arr[ready_queue.head];
            ready_queue.head = (ready_queue.head +1) % THREAD_MAX;
            ready_queue.size--;
            longjmp(current_thread->env, 1);    
        }
        if(idle_thread && setjmp(idle_thread->env) == 0) {
            current_thread = idle_thread;
            longjmp(idle_thread->env, 1);
        }
    }
}

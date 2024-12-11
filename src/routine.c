#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "thread_tool.h"

void idle(int id, int *args) {
    // start with thread setup
    //perror("idle: before thread_setup\n");
    thread_setup(id, args);
    //perror("idel: after thread_setup\n");

    // step 1: print idle message
    printf("thread %d: idle\n", id);

    // the idle routine runs indefinitely
    while(1) {
        // step 2: sleep for 1 second
        sleep(1);

        // step 3: yield the thread
        thread_yield();
    }
}

void fibonacci(int id, int *args) {
    // step 1: set up the thread
    thread_setup(id, args);

    // step 2: parse the argument
    current_thread->n = current_thread->args[0];
    
    // step 3: compute Fibonacci numbers
    for (current_thread->i = 1;; current_thread->i++) {
        if (current_thread->i <= 2) {
            current_thread->f_cur = 1;
            current_thread->f_prev = 1;
        } else {
            int f_next = current_thread->f_cur + current_thread->f_prev;
            current_thread->f_prev = current_thread->f_cur;
            current_thread->f_cur = f_next;
        }

        // step 4: print the current Fibonnaci number 
        printf("thread %d: F_%d = %d\n", current_thread->id, current_thread->i,
               current_thread->f_cur);

        // step 5: sleep for synchronization
        sleep(1);

        // step 6: yield or exit
        if (current_thread->i == current_thread->n) {
            thread_exit();
        } else {
            thread_yield();
        }
    }
}

void pm(int id, int *args) {
    // thread 1: set up the thread 
    thread_setup(id, args);

    // step 2: parse the argument
    current_thread->n = current_thread->args[0];

    // step 3: initialize pm_value for the thread
    current_thread->pm_value = 1; // pm(1) = 1

    // step 4: compute plus-minus values
    for(current_thread->i = 1; current_thread->i <= current_thread->n; current_thread->i++) {
        if (current_thread->i > 1) {
            current_thread->pm_value = ((current_thread->i % 2 == 0)? -1: 1) * current_thread->i + current_thread->pm_value;
        }

        // step 5: print the current pm value
        printf("thread %d: pm(%d) = %d\n", current_thread->id, current_thread->i, current_thread->pm_value);
    
        // step 6: sleep for synchronization
        sleep(1);

        // step 7: yield or exit
        if (current_thread->i == current_thread->n) {
            thread_exit();
        } else {
            thread_yield();
        }
    }
}

void enroll(int id, int *args) {
    // Step 1: Setup the thread
    thread_setup(id, args);

    // Parse arguments and save them in the thread's tcb
    current_thread->dp = current_thread->args[0]; // Desire for pj_class
    current_thread->ds = current_thread->args[1]; // Desire for sw_class
    current_thread->sleep_time = current_thread->args[2]; // Sleep time
    current_thread->best_friend_id = current_thread->args[3]; // Best friend's thread ID

    // Step 2: Simulate oversleeping
    printf("thread %d: sleep %d\n", current_thread->id, current_thread->sleep_time);
    thread_sleep(current_thread->sleep_time);

    // Step 3: Wake up best friend
    thread_awake(current_thread->best_friend_id);

    // Acquire the read lock and record class quotas
    read_lock();
    printf("thread %d: acquire read lock\n", current_thread->id);

    // Save current quotas in the thread's tcb
    current_thread->qp = q_p; // Remaining spots in pj_class
    current_thread->qs = q_s; // Remaining spots in sw_class

    sleep(1); // Synchronization sleep
    thread_yield();

    // Step 4: Release the read lock and compute priorities
    read_unlock();
    current_thread->pp = current_thread->dp * current_thread->qp; // Priority for pj_class
    current_thread->ps = current_thread->ds * current_thread->qs; // Priority for sw_class
    printf("thread %d: release read lock, p_p = %d, p_s = %d\n", current_thread->id, current_thread->pp, current_thread->ps);

    sleep(1); // Synchronization sleep
    thread_yield();

    // Step 5: Acquire the write lock and attempt to enroll
    write_lock();

    if ((current_thread->pp > current_thread->ps) || 
        (current_thread->pp == current_thread->ps && current_thread->dp > current_thread->ds)) {
        // Prefer pj_class
        if (current_thread->qp > 0) {
            q_p--; // Enroll in pj_class
            current_thread->class_enrolled = "pj_class";
        } else {
            q_s--; // pj_class full, enroll in sw_class
            current_thread->class_enrolled = "sw_class";
        }
    } else {
        // Prefer sw_class
        if (current_thread->qs > 0) {
            q_s--; // Enroll in sw_class
            current_thread->class_enrolled = "sw_class";
        } else {
            q_p--; // sw_class full, enroll in pj_class
            current_thread->class_enrolled = "pj_class";
        }
    }

    printf("thread %d: acquire write lock, enroll in %s\n", current_thread->id, current_thread->class_enrolled);

    sleep(1); // Synchronization sleep
    thread_yield();

    // Step 6: Release the write lock and exit
    write_unlock();
    printf("thread %d: release write lock\n", current_thread->id);

    sleep(1); // Synchronization sleep
    thread_exit();
}


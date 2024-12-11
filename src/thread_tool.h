#ifndef THREAD_TOOL_H
#define THREAD_TOOL_H

#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>

// The maximum number of threads.
#define THREAD_MAX 100
#define FROM_thread_yield 1
#define FROM_read_lock 2
#define FROM_write_lock 3
#define FROM_thread_sleep 4
#define FROM_thread_exit 5
#define FROM_sighandler 6

#define FROM_scheduler 9

#define DEBUG 0

void sighandler(int signum);
void scheduler();

// The thread control block structure.
struct tcb {
    int id;
    int *args;
    // Reveals what resource the thread is waiting for. The values are:
    //  - 0: no resource.
    //  - 1: read lock.
    //  - 2: write lock.
    int waiting_for;
    int sleeping_time;
    jmp_buf env;  // Where the scheduler should jump to.
    int n, i, f_cur, f_prev; // TODO: Add some variables you wish to keep between switches.
    int pm_value; // plus-minus value

    // Variables for Enrollment routine
    int dp, ds, sleep_time, best_friend_id; // Parsed arguments
    int qp, qs;                             // Recorded quotas
    int pp, ps;                             // Computed priorities
    char *class_enrolled;             // Class enrollment status
};

// The only one thread in the RUNNING state.
extern struct tcb *current_thread;
extern struct tcb *idle_thread;

struct tcb_queue {
    struct tcb *arr[THREAD_MAX];  // The circular array.
    int head;                     // The index of the head of the queue
    int tail;
    int size;//may not be used in the future
};

extern struct tcb_queue ready_queue, waiting_queue;
extern struct tcb *sleeping_set[THREAD_MAX];


// The rwlock structure.
//
// When a thread acquires a type of lock, it should increment the corresponding count.
struct rwlock {
    int read_count;
    int write_count;
};

extern struct rwlock rwlock;

// The remaining spots in classes.
extern int q_p, q_s;

// The maximum running time for each thread.
extern int time_slice;

// The long jump buffer for the scheduler.
extern jmp_buf sched_buf;

#define DEBUGLOG(fmt, ...) \
    ({ \
        if (DEBUG) \
            printf("\033[1;34m[%s:%d:%s][tid:%d]: " fmt "\033[0m\n", \
                    __FILE__, __LINE__, __func__, \
                    current_thread ? current_thread->id : -1, \
                    ##__VA_ARGS__); \
    })

// TODO::
// You should setup your own sleeping set as well as finish the marcos below
#define thread_create(func, t_id, t_args) \
    func(t_id, t_args)

#define thread_setup(t_id, t_args)                                                  \
    ({                                                                              \
        struct tcb *new_thread = (struct tcb*)malloc(sizeof(struct tcb));           \
        if(!new_thread) {                                                           \
            perror("Failed to allocate memory for TCB.\n");                         \
            exit(1);                                                                \
        }                                                                           \
        new_thread->id = t_id;                                                      \
        new_thread->args = t_args;                                                  \
        if(!setjmp(new_thread->env)) {                                              \
            printf("thread %d: set up routine %s\n", t_id, __func__);               \
            if(t_id != 0){                                                          \
                if((ready_queue.tail +1) % THREAD_MAX == ready_queue.head){         \
                    fprintf(stderr, "ready_queue overflow.\n");                     \
                    exit(1);                                                        \
                }                                                                   \
                ready_queue.arr[ready_queue.tail] = new_thread;                     \
                ready_queue.tail = (ready_queue.tail +1 ) % THREAD_MAX;             \
                ready_queue.size++;                                                 \
            } else {                                                                \
                idle_thread = new_thread;                                           \
            }                                                                       \
            return;                                                                 \
        } else {\
            if (DEBUG) printf("In Thread_setup: setjmp else. cur thr id:%d\n", current_thread->id);\
        }                                                                          \
    })

#define thread_yield()                                                              \
    ({                                                                              \
        if (setjmp(current_thread->env) == 0) {                                     \
            if (DEBUG) printf("Thread_Yield: in setjmp0 . Cur thr id:%d\n", current_thread->id);\
            /* unblock SIGTSTP */                                                   \
            sigset_t sig_unblock_tstp;                                              \
            sigemptyset(&sig_unblock_tstp);                                         \
            sigaddset(&sig_unblock_tstp, SIGTSTP);                                  \
            sigprocmask(SIG_UNBLOCK, &sig_unblock_tstp, NULL);                      \
                                                                                    \
            /* block SIGTSTP */                                                     \
            sigset_t sig_block_tstp;                                                \
            sigemptyset(&sig_block_tstp);                                           \
            sigaddset(&sig_block_tstp, SIGTSTP);                                    \
            sigprocmask(SIG_BLOCK, &sig_block_tstp, NULL);                          \
                                                                                    \
            /* unblock SIGALRM */                                                   \
            sigset_t sig_unblock_alrm;                                              \
            sigemptyset(&sig_unblock_alrm);                                         \
            sigaddset(&sig_unblock_alrm, SIGALRM);                                  \
            sigprocmask(SIG_UNBLOCK, &sig_unblock_alrm, NULL);                      \
                                                                                    \
            /* block SIGALRM */                                                     \
            sigset_t sig_block_alrm;                                                \
            sigemptyset(&sig_block_alrm);                                           \
            sigaddset(&sig_block_alrm, SIGALRM);                                    \
            sigprocmask(SIG_BLOCK, &sig_block_alrm, NULL);                          \
        } else {                                                                    \
            if (DEBUG) printf("Thread_Yield: in setjmp else, cur_thr_id:%d\n", current_thread->id);\
        }                                                                         \
    })

#define read_lock()                                                                 \
    ({                                                                              \
        while(1){                                                                   \
            if(rwlock.write_count == 0) {                                           \
                rwlock.read_count++;                                                \
                break;                                                              \
            } else {                                                                \
                if (setjmp(current_thread->env) == 0) {                             \
                    longjmp(sched_buf, FROM_read_lock);                             \
                } else {                                                            \
                    break;                                                          \
                }                                                                   \
            }                                                                       \
        }                                                                           \
    })

#define write_lock()                                                                \
    ({                                                                              \
        while(1) {                                                                  \
            if(rwlock.read_count == 0 && rwlock.write_count == 0) {                 \
                rwlock.write_count++;                                               \
                break;                                                              \
            } else {                                                                \
                if (setjmp(current_thread->env) == 0) {                             \
                    longjmp(sched_buf, FROM_write_lock);                            \
                } else {                                                            \
                    break;                                                          \
                }                                                                   \
            }                                                                       \
        }                                                                           \
    })

#define read_unlock()                                                               \
    ({                                                                              \
        rwlock.read_count--;                                                       \
    })

#define write_unlock()                                                              \
    ({                                                                              \
        rwlock.write_count--;                                                      \
    })

#define thread_sleep(sec)                                                           \
    ({                                                                              \
        if (sec <1 || sec >10) {                                                    \
            fprintf(stderr, "thread_sleep: Invalid sleep duration\n");              \
            exit(1);                                                                \
        }                                                                           \
        if(setjmp(current_thread->env) == 0){                                       \
            current_thread->sleeping_time = sec * time_slice;                       \
            sleeping_set[current_thread->id] = current_thread;                      \
            current_thread = NULL;                                                  \
            longjmp(sched_buf, FROM_thread_sleep);                                  \
        }                                                                           \
    })

#define thread_awake(t_id)                                                          \
    ({                                                                              \
        struct tcb* thread = sleeping_set[t_id];                                    \
        if (thread) {                                                               \
            sleeping_set[t_id] = NULL;                                              \
            ready_queue.arr[ready_queue.tail] = thread;                             \
            ready_queue.tail = (ready_queue.tail +1) % THREAD_MAX;                  \
            ready_queue.size++;                                                     \
        }                                                                           \
    })

#define thread_exit()                                                               \
    ({                                                                              \
        printf("thread %d: exit\n", current_thread->id);                            \
        longjmp(sched_buf, FROM_thread_exit);                                       \
    })

#endif  // THREAD_TOOL_H

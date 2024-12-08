#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "routine.h"
#include "thread_tool.h"

// Global variable.

struct tcb *current_thread, *idle_thread;
struct tcb_queue ready_queue, waiting_queue;
struct rwlock rwlock;
int q_p, q_s;
int time_slice;
jmp_buf sched_buf;

// Prints `msg`, the error message from `errno`, and exits the program with status 1.
void perror_exit(const char *msg) {
    perror(msg);
    exit(1);
}

// Turns stdin, stdout and stderr into unbuffered I/O, so that:
//   - you see everything your program prints in case it crashes.
//   - the program behaves the same if its stdout doesn't connect to a terminal.
void unbuffered_io(void) {
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
}

// Initializes the signal masks and the signal handler.
void init_signal(void) {
    // Initializes the signal masks containing both SIGTSTP and SIGALRM.
    sigset_t tstp_alrm_mask;
    if (sigemptyset(&tstp_alrm_mask) == -1) perror_exit("tstp_alrm_mask sigemptyset");
    if (sigaddset(&tstp_alrm_mask, SIGTSTP) == -1) perror_exit("tstp_alrm_mask sigaddset SIGTSTP");
    if (sigaddset(&tstp_alrm_mask, SIGALRM) == -1) perror_exit("tstp_alrm_mask sigaddset SIGALRM");

    // Blocks both SIGTSTP and SIGALRM.
    if (sigprocmask(SIG_BLOCK, &tstp_alrm_mask, NULL) == -1)
        perror_exit("sigprocmask SIG_BLOCK tstp_alrm");

    // Set the signal handler for both SIGTSTP and SIGALRM.
    struct sigaction sa;
    sa.sa_handler = sighandler;
    sa.sa_flags = 0;
    if (sigemptyset(&sa.sa_mask) == -1) perror_exit("sa.sa_mask sigemptyset");
    if (sigaddset(&sa.sa_mask, SIGTSTP) == -1) perror_exit("sa.sa_mask sigaddset tstp");
    if (sigaddset(&sa.sa_mask, SIGALRM) == -1) perror_exit("sa.sa_mask sigaddset alrm");

    if (sigaction(SIGTSTP, &sa, NULL) == -1) perror_exit("sigaction SIGTSTP");
    if (sigaction(SIGALRM, &sa, NULL) == -1) perror_exit("sigaction SIGALRM");
}

void spawn_thread(int argc, char *argv[]) {
    char *padding[128];
    (void)
        padding;  // A common way to prevents the compiler from complaining about unused variables.

    int argv_idx = 4;
    int thread_id = 1;
    while (argv_idx < argc) {
        void (*routine)(int, int *);
        int *thread_arg;

        int routine_type = atoi(argv[argv_idx++]);
        if (routine_type == 1) {
            if (argv_idx >= argc) {
                fprintf(stderr, "usage: thread %d: <n> is not provided\n", thread_id);
                exit(1);
            }

            routine = fibonacci;

            thread_arg = calloc(1, sizeof(int));
            thread_arg[0] = atoi(argv[argv_idx++]);
        } else if (routine_type == 2) {
            if (argv_idx >= argc) {
                fprintf(stderr, "usage: thread %d: <n> is not provided\n", thread_id);
                exit(1);
            }

            routine = pm;

            thread_arg = calloc(1, sizeof(int));
            thread_arg[0] = atoi(argv[argv_idx++]);
        } else if (routine_type == 3) {
            if (argv_idx + 3 >= argc) {
                fprintf(stderr, "usage: thread %d: <d_p> <d_s> <s> <b> are not provided\n",
                        thread_id);
                exit(1);
            }

            routine = enroll;

            thread_arg = calloc(4, sizeof(int));
            thread_arg[0] = atoi(argv[argv_idx++]);
            thread_arg[1] = atoi(argv[argv_idx++]);
            thread_arg[2] = atoi(argv[argv_idx++]);
            thread_arg[3] = atoi(argv[argv_idx++]);
        } else {
            fprintf(stderr, "usage: thread %d: unrecognized routine type %d\n", thread_id,
                    routine_type);
            exit(1);
        }
        thread_create(routine, thread_id++, thread_arg);
    }
}

void start_threading() {
    scheduler();
}

int main(int argc, char *argv[]) {    
    unbuffered_io();
    init_signal();

    if (argc < 4) {
        fprintf(stderr, "usage: <time_slice> <q_p> <q_s> are not provided\n");
        exit(1);
    }

    // parse arguments for time slice and quotes
    time_slice = atoi(argv[1]);
    q_p = atoi(argv[2]);
    q_s = atoi(argv[3]);

    if (argc < 5) {
        fprintf(stderr, "usage: at least one thread is required\n");
        exit(1);
    }

    // Spawn threads using the global ready queue
    spawn_thread(argc, argv);

    // start threading
    start_threading();
}

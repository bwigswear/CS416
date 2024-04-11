#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1000

#define STACK_SIZE SIGSTKSZ
#define QUANTUM 5

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/queue.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

typedef uint worker_t;

TAILQ_HEAD(tailhead, entry);

typedef struct entry {
	int thread;
	TAILQ_ENTRY(entry) entries;
} entry;

typedef struct TCB {
	ucontext_t thread_context;
	int thread_id;
	int thread_status;
	// 0 - RUNNING, 1 - READY, 2 - BLOCKED. 3 - FINISHED
	int thread_priority;
	int initialized; // needed this for setting up scheduler tcb, there's prob a better way
	int joined_worker;
	void* return_val;
	entry* entry_ptr;
	int quantums_used;
	int time_used;
	struct timeval arrival;
	int response;
	int ending_method;
} tcb; 

/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */
	int initialized;
	atomic_flag lock;
	worker_t owner;
	struct tailhead blocked;
	// YOUR CODE HERE
} worker_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

#define MAX_PRIORITY 4
#define MIN_PRIORITY 1

/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);

/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

#endif

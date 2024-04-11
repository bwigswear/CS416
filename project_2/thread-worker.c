// File:	thread-worker.c

// bcw62 - Ben Wiggins
// kpl56 - Keith Lehman
// Used kill.cs.rutgers.edu
// CS 416

#include "thread-worker.h"
#include <ucontext.h>
#include <string.h>


//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;

// INITAILIZE ALL YOUR OTHER VARIABLES HERE

struct itimerval timer;
/*timer.it_interval.tv_sec = 0;
timer.it_interval.tv_usec = 0;
timer.it_value.tv_usec = 0;
timer.it_value.tv_sec = QUANTUM / 1000;*/

//scheduler tcb (not gonna be part of linked list)
tcb scheduler = {
    .initialized = 0
};

int tcb_counter = 1;
tcb* all_tcbs; 
worker_t curr_tcb_index;

//TAILQ_HEAD(tailhead, entry) queue_head;
//^^Moved macro to .h file and kept declaration here
struct tailhead queue_head;

static void timer_interrupt(int signum);
static void schedule();

int tqs_since_last_reset = 0;

int scheduler_create(){
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	timer.it_value.tv_usec = QUANTUM * 1000;
	timer.it_value.tv_sec = 0;
	//add one to tcb counter and malloc array

	//since each new worker also reallocs an additional spot in this array, index 0 will be empty
	//we could possibly use this for the scheduler or benchmark context
	all_tcbs = (tcb*) malloc(sizeof(tcb) * ++tcb_counter);

	//setup scheduler tcb
	scheduler.initialized = 1; 

	//save main context
	ucontext_t main;
	getcontext(&main);

	//setup scheduler context
	//void* stack = malloc(STACK_SIZE);
	getcontext(&scheduler.thread_context); // initialize the context with the current thread's context
	scheduler.thread_context.uc_link=NULL;
	scheduler.thread_context.uc_stack.ss_sp = (void*) malloc(STACK_SIZE);
	scheduler.thread_context.uc_stack.ss_size = STACK_SIZE;
	scheduler.thread_context.uc_stack.ss_flags=0;
	makecontext(&scheduler.thread_context,  &schedule, 1, NULL); // create the new context


	//setup queue
	TAILQ_INIT(&queue_head);

	struct sigaction handler;
	memset(&handler, 0, sizeof(handler));
	handler.sa_handler = &timer_interrupt;
	//sigemptyset(&handler.sa_mask);
	sigaction(SIGPROF, &handler, NULL);

	//realloc tcb array
	//all_tcbs = realloc(all_tcbs, sizeof(tcb) * ++tcb_counter);
	//printf("TCB array realloced.\n");
	
	//create new tcb
	int new_ctxt_id = tcb_counter - 1;
	all_tcbs[new_ctxt_id].thread_context = main;
	all_tcbs[new_ctxt_id].thread_priority = MAX_PRIORITY;
	all_tcbs[new_ctxt_id].thread_id = (worker_t) new_ctxt_id;
	all_tcbs[new_ctxt_id].thread_status = 1;
	all_tcbs[new_ctxt_id].joined_worker = 0;
	all_tcbs[new_ctxt_id].quantums_used = 0;
	all_tcbs[new_ctxt_id].response = 0;
	gettimeofday(&all_tcbs[new_ctxt_id].arrival, NULL);
	//printf("New TCB created with thread id: %d.\n", new_ctxt_id);

	//add new tcb to queue
	entry *new_entry = malloc(sizeof(entry));
	new_entry->thread = (worker_t) new_ctxt_id;
	TAILQ_INSERT_TAIL(&queue_head, new_entry, entries);
	//printf("New TCB added to queue.\n");

	all_tcbs[new_ctxt_id].entry_ptr = new_entry;

	curr_tcb_index = new_ctxt_id;

	
}

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	
	// - create Thread Control Block (TCB)	
	// - create and initialize the context of this worker thread
	// - allocate space of stack for this thread to run
	// after everything is set, push this thread into run queue and 
	// - make it ready for the execution.
	if(scheduler.initialized == 0){
		scheduler_create();
		worker_create(thread, attr, function, arg);
	}else{
		//create new context
		ucontext_t new_context;
		getcontext(&new_context);
		new_context.uc_stack.ss_sp = (void*) malloc(STACK_SIZE);
		new_context.uc_stack.ss_size = STACK_SIZE;
		new_context.uc_link = &all_tcbs[0].thread_context;
		new_context.uc_flags = 0;
		makecontext(&new_context, function, 1, NULL); // create the new context
		//printf("New context created.\n");
		//realloc tcb array
		all_tcbs = realloc(all_tcbs, sizeof(tcb) * ++tcb_counter);

		//printf("TCB array realloced.\n");
		
		//create new tcb
		int new_ctxt_id = tcb_counter - 1;
		*thread = new_ctxt_id;
		all_tcbs[new_ctxt_id].thread_context = new_context;
		all_tcbs[new_ctxt_id].thread_id = (worker_t) new_ctxt_id;
		all_tcbs[new_ctxt_id].thread_status = 1;
		all_tcbs[new_ctxt_id].thread_priority = MAX_PRIORITY;
		all_tcbs[new_ctxt_id].joined_worker = 0;
		all_tcbs[new_ctxt_id].quantums_used = 0;
		all_tcbs[new_ctxt_id].response = 0;
		gettimeofday(&all_tcbs[new_ctxt_id].arrival, NULL);
		//printf("New TCB created with thread id: %d.\n", new_ctxt_id);

		//add new tcb to queue
		entry *new_entry = malloc(sizeof(entry));
		new_entry->thread = (worker_t) new_ctxt_id;
		TAILQ_INSERT_TAIL(&queue_head, new_entry, entries);
		//printf("New TCB added to queue.\n");
		all_tcbs[new_ctxt_id].entry_ptr = new_entry;

		tot_cntx_switches++;
		//swap to scheduler
		//printf("Swapping to scheduler...In thread %d\n", curr_tcb_index);
		swapcontext(&all_tcbs[curr_tcb_index].thread_context, &scheduler.thread_context);

	}
    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	//saves current context to curr tcb and swap to scheduler context
	//int x = (int) curr_tcb_index;
	//printf("Worker yield called from %d.\n", curr_tcb_index);
	//printf("size of context: %d\n", sizeof(ucontext_t));

	tot_cntx_switches++;
	all_tcbs[curr_tcb_index].ending_method = 2;
	all_tcbs[curr_tcb_index].thread_status = 1;
	swapcontext(&all_tcbs[curr_tcb_index].thread_context, &scheduler.thread_context);
};

/* terminate a thread */
void worker_exit(void *value_ptr) {

	//printf("Worker exit called.\n");
	// - de-allocate any dynamic memory created when starting this thread

	free(all_tcbs[curr_tcb_index].thread_context.uc_stack.ss_sp);
	//free( TCB from scheduling data structure)
	//remove TCB pointer from scheduling data structure

	if(all_tcbs[curr_tcb_index].joined_worker != 0){
		//printf("there is something joined to me\n");
		all_tcbs[all_tcbs[curr_tcb_index].joined_worker].thread_status = 1;
		TAILQ_INSERT_TAIL(&queue_head, all_tcbs[all_tcbs[curr_tcb_index].joined_worker].entry_ptr, entries);
	}else{
		//printf("join var: %d\n", all_tcbs[curr_tcb_index].joined_worker);
	}

	entry *curr_node = TAILQ_FIRST(&queue_head);
	//printf("Exit removing %d\n", curr_node->thread);
	if (curr_node != NULL) {
		TAILQ_REMOVE(&queue_head, curr_node, entries);
	}

	struct timeval current;
	struct timeval arrival_time = all_tcbs[curr_tcb_index].arrival;
	gettimeofday(&current, NULL);
	if(tcb_counter != 1){avg_turn_time *= (tcb_counter - 1);}
	avg_turn_time+=(1000000 * ((float)current.tv_sec - (float)arrival_time.tv_sec) + ((float)current.tv_usec - (float)arrival_time.tv_usec)); 	
	avg_turn_time /= tcb_counter;
	//printf("Avg turnaround time%f\n", avg_turn_time);


	//printf("Thread %d has used %d quantums\n", curr_tcb_index, all_tcbs[curr_tcb_index].quantums_used);
	tot_cntx_switches++;
	all_tcbs[curr_tcb_index].thread_status = 3;
	all_tcbs[curr_tcb_index].return_val = value_ptr;
	swapcontext(&all_tcbs[curr_tcb_index].thread_context, &scheduler.thread_context);
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	if(all_tcbs[thread].thread_status != 3){
		//printf("joining %d to %d\n", curr_tcb_index, thread);
		all_tcbs[thread].joined_worker = curr_tcb_index;
		all_tcbs[curr_tcb_index].thread_status = 2;
		tot_cntx_switches++;
		swapcontext(&all_tcbs[curr_tcb_index].thread_context, &scheduler.thread_context);

	}else if(value_ptr != NULL){
		value_ptr = &all_tcbs[thread].return_val;
	}

	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex
	mutex->initialized = 1;
	atomic_flag init = ATOMIC_FLAG_INIT;
	mutex->lock = init;
	mutex->owner = NULL;
	TAILQ_INIT(&mutex->blocked);
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {
	

	if(atomic_flag_test_and_set(&mutex->lock)){
		tot_cntx_switches++;
		all_tcbs[curr_tcb_index].thread_status = 2;//Block current tcb if mutex is locked
		entry* new_entry = malloc(sizeof(entry));
		new_entry->thread = curr_tcb_index;
		TAILQ_INSERT_HEAD(&mutex->blocked, new_entry/*all_tcbs[curr_tcb_index].entry_ptr*/, entries);
		swapcontext(&all_tcbs[curr_tcb_index].thread_context, &scheduler.thread_context);
	}
	
	if(mutex->owner != NULL){
		//printf("Flag was not set, but mutex had an owner");
		return -1;
	}

	mutex->owner = all_tcbs[curr_tcb_index].thread_id;
        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	
	if(mutex->owner != all_tcbs[curr_tcb_index].thread_id){
		return -1;
	}

	atomic_flag_clear(&mutex->lock);
	mutex->owner = NULL;
	
	entry* traverse;
	TAILQ_FOREACH(traverse, &mutex->blocked, entries){
		//printf("Added back to queue\n");
		all_tcbs[traverse->thread].thread_status = 1;
		TAILQ_INSERT_TAIL(&queue_head, traverse, entries);
	}

	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.
	
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	atomic_flag_clear(&mutex->lock);
	entry** freestuff = malloc(sizeof(entry*) * 64);
	int i = 0;
	entry* traverse;
	TAILQ_FOREACH(traverse, &mutex->blocked, entries){
		//printf("Added back to queue\n");
		all_tcbs[traverse->thread].thread_status = 1;
		TAILQ_INSERT_TAIL(&queue_head, traverse, entries);
		freestuff[i] = traverse;
		i++;
	}
	for(int j = 0; j < i; j++){free(freestuff[j]);}
	free(freestuff);
	return 0;
};

static void sched_psjf();
static void sched_mlfq();

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ)

	// 

	//for now, every time the scheduler is called (regardless of the time spent on the curr process) it will
	//move the current process to the end of the queue and swap in the next process
	//printf("Scheduler called\n");
	entry *curr_entry = TAILQ_FIRST(&queue_head);
	if (curr_entry != NULL){
		if(curr_entry->thread == all_tcbs[curr_tcb_index].thread_id){
			if(all_tcbs[curr_tcb_index].thread_status == 2 || all_tcbs[curr_tcb_index].thread_status == 3){
				TAILQ_REMOVE(&queue_head, curr_entry, entries);
			}else{
				all_tcbs[curr_tcb_index].thread_status = 1;
				TAILQ_REMOVE(&queue_head, curr_entry, entries);
				TAILQ_INSERT_TAIL(&queue_head, curr_entry, entries);
			}
		}else{
			//printf("front of queue different from curr_tcb_index\n");
		}


		#ifndef MLFQ
    			//printf("===== Running PSJF =====\n");
			sched_mlfq();
		#else
    			//printf("===== Running MLFQ =====\n");
			sched_psjf();
		#endif

		// Code to execute if SCHED is not set or set to a different value

		struct itimerval* old_time;
		setitimer(ITIMER_PROF, &timer, old_time);
		//printf("Swapping to thread %d\n", curr_tcb_index);
		tot_cntx_switches++;
		if(!all_tcbs[curr_tcb_index].response){
			all_tcbs[curr_tcb_index].response = 1;
			struct timeval current;
			struct timeval arrival_time = all_tcbs[curr_tcb_index].arrival;
			gettimeofday(&current, NULL);
			if(tcb_counter != 1){avg_resp_time *= (tcb_counter - 1);}
			avg_resp_time+=(1000000 * ((float)current.tv_sec - (float)arrival_time.tv_sec) + ((float)current.tv_usec - (float)arrival_time.tv_usec));
			avg_resp_time /= tcb_counter;
		}
		//printf("Avg response time: %f\n", avg_resp_time);
		setcontext(&all_tcbs[curr_tcb_index].thread_context);

	}
	
}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)
	
	int least_tqs = 999999999;
	entry *next_entry;
	entry *x;
	TAILQ_FOREACH(x, &queue_head, entries) {
		int thread = x->thread;
		if(all_tcbs[thread].quantums_used < least_tqs){
			least_tqs = all_tcbs[thread].quantums_used;
			next_entry = x;
		}
	}
	TAILQ_REMOVE(&queue_head, next_entry, entries);
	TAILQ_INSERT_HEAD(&queue_head, next_entry, entries);
	curr_tcb_index = TAILQ_FIRST(&queue_head)->thread;
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	int S = 5;
	if(tqs_since_last_reset > S){
		//set all priorities to highest and continue with head
		tqs_since_last_reset = 0;
		entry *x;
		TAILQ_FOREACH(x, &queue_head, entries) {
			int thread = x->thread;
			all_tcbs[thread].thread_priority = MAX_PRIORITY;
			all_tcbs[thread].ending_method = 0;
			all_tcbs[thread].time_used = 0;

		}
	}else{
		//find first entry with highest priority
		tqs_since_last_reset += 1;
		entry *x;
		int highest_priority = 0;
		entry *highest_priority_entry;
		TAILQ_FOREACH(x, &queue_head, entries) {
			int thread = x->thread;
			//handle priority lowering
			if(all_tcbs[thread].ending_method != 0){
				if(all_tcbs[thread].ending_method == 1){
					//thread used its entire last tq
					all_tcbs[thread].time_used += QUANTUM;
				}else{
					//thread yielded its time
					//all_tcbs[thread].time_used += [timer amount used]
				}
				if((all_tcbs[thread].ending_method == 1 || all_tcbs[thread].time_used > (QUANTUM * 5)) && all_tcbs[thread].thread_priority > MIN_PRIORITY){
					//printf("Priority of thread %d: %d\n", thread, all_tcbs[thread].thread_priority);
					all_tcbs[thread].thread_priority--;
					all_tcbs[thread].time_used = 0;
					//printf("New priority of thread %d: %d\n", thread, all_tcbs[thread].thread_priority);
				}
				all_tcbs[thread].ending_method = 0;
			} 
			if(all_tcbs[thread].thread_priority > highest_priority){
				highest_priority = all_tcbs[thread].thread_priority;
				highest_priority_entry = x;
			}
		}
		TAILQ_REMOVE(&queue_head, highest_priority_entry, entries);
		TAILQ_INSERT_HEAD(&queue_head, highest_priority_entry, entries);
	}
	//at this point HEAD will be run next
	curr_tcb_index = TAILQ_FIRST(&queue_head)->thread;
	all_tcbs[curr_tcb_index].ending_method = 1;
	//printf("Swapping to thread %d\n", curr_tcb_index);
	//setcontext(&all_tcbs[curr_tcb_index].thread_context);
}

//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}

void timer_interrupt(int signum){
	//printf("Timer interrupt\n");
	tot_cntx_switches++;
	all_tcbs[curr_tcb_index].quantums_used++;
	swapcontext(&all_tcbs[curr_tcb_index].thread_context, &scheduler.thread_context);
}


// Feel free to add any other functions you need

// YOUR CODE HERE


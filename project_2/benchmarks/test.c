#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../thread-worker.h"
#include "../thread-worker.c"

/* A scratch program template on which to call and
 * test thread-worker library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */

worker_mutex_t z;

void* my_third_func(void* arg){
    //printf("In func C\n");
    worker_exit(NULL);
}

void* my_function(void* arg) {
	worker_mutex_lock(&z);
    // thread function code goes here
    //printf("A 1\n");
	worker_yield();
    //printf("A creating a child thread.\n");
    pthread_t test;
    pthread_create(&test, NULL, &my_third_func, NULL);
    /*while(1){

    }*/
    for(int i = 0; i < 9999999; i++){
    }

    //printf("mutex owner: %d\n", z.owner);
    //printf("A 2\n");
    worker_yield();
    //printf("A 3\n");
    worker_mutex_unlock(&z);
    worker_exit(NULL);
    return 0;
}

void* my_other_function(void* arg) {
    // thread function code goes here
    worker_mutex_lock(&z);
    //printf("B 1\n");
	worker_yield();
    //printf("B 2\n");
    worker_yield();
    //printf("B 3\n");
    worker_exit(NULL);
    return 0;
}

void* thisthing(void* arg){
	//printf("C 1\n");
	worker_exit(NULL);
}

int main(int argc, char **argv) {

worker_mutex_init(&z, NULL);
    pthread_t a;
    pthread_t b;
	//printf("starting main.\n");
    pthread_create(&a, NULL, &my_function, NULL);
    //printf("a: %d\n", a);
    pthread_create(&b, NULL, &my_other_function, NULL);
    //printf("b: %d\n", b);
    void* x;
    worker_join(b, x);
    pthread_t c;
    pthread_create(&c, NULL, &thisthing, NULL);
    printf("end of main\n");
    return 0;
}



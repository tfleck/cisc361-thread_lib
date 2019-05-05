/*
 * types used by thread library
 */
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/mman.h>

typedef struct tcb_t
{
    //TCB containing all relevant information about the thread
    int thread_id;
    int thread_priority;
	ucontext_t *thread_context;
	struct tcb_t *next;
} tcb_t;

typedef struct tQueue_t
{
    //pointers to start and end of list
    tcb_t *head, *tail;
} tQueue_t;

//Thread library fns
void t_init();
void t_create(void(*function)(int), int thread_id, int priority);
void t_yield();
void t_terminate();
void t_shutdown();

//Internal queueing fns
tQueue_t* createQueue();
void addQueue(tQueue_t *q, tcb_t *t);
tcb_t* rmQueue(tQueue_t *q);

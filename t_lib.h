/*
 * types used by thread library
 */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/time.h>

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

typedef struct sem_t
{
  int count;
  tQueue_t *q;
} sem_t;

//Thread library fns
void t_init();
void t_create(void(*function)(int), int thread_id, int priority);
void t_yield();
void t_terminate();
void t_shutdown();

//Semaphore fns
void sem_init(sem_t **sp, int sem_count);
void sem_wait(sem_t *sp);
void sem_signal(sem_t *sp);
void sem_destroy(sem_t **sp);

//Internal scheduling fns
void sig_handler();
void init_alarm();

//Internal queueing fns
tQueue_t* createQueue();
void addQueue(tQueue_t *q, tcb_t *t);
tcb_t* rmQueue(tQueue_t *q);

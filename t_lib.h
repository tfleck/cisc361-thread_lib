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
#include <string.h>

typedef struct tcb_t
{
  //TCB containing all relevant information about the thread
  int thread_id;
  int thread_priority;
  ucontext_t *thread_context;
  struct mbox *mail;
	struct tcb_t *next;
	struct tcb_t *next_all;
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

typedef struct messageNode
{
  char *message;            // copy of the message
  int len;                  // length of the message
  int sender;               // TID of the sender thread
  int receiver;             // TID of the receiver thread
  sem_t *recv_wait;          // Threads waiting for message to be received
  struct messageNode *next; // pointer to next node
} messageNode;

typedef struct mbox
{
  messageNode *msg; // message queue
  sem_t *mbox_send; // threads waiting to edit mailbox
  sem_t *mbox_recv; // threads waiting to receive from mailbox
} mbox;

//External Funtions

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

//Mailbox fns
void mbox_create(mbox **mb);
void mbox_destroy(mbox **mb);
void mbox_deposit(mbox *mb, char *msg, int len);
void mbox_withdraw(mbox *mb, char *msg, int *len);

//Message fns
void send(int tid, char *msg, int len);
void receive(int *tid, char *msg, int *len);
void block_send(int tid, char *msg, int len);
void block_receive(int *tid, char *msg, int *len);

//Internal Functions

//Internal scheduling fns
void sig_handler();
void init_alarm();

//Internal queueing fns
tQueue_t* createQueue();
void addQueue(tQueue_t *q, tcb_t *t);
tcb_t* rmQueue(tQueue_t *q, int tid);
tcb_t* findById(tQueue_t *q, int tid);

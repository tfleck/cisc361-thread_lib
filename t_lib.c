#include "t_lib.h"

tQueue_t *running;
tQueue_t *ready;

void t_init()
{
  //Initialize queues
  ready = createQueue();
  running = createQueue();

  //Create TCB for main thread
  tcb_t *tmp = (tcb_t *) calloc(1,sizeof(tcb_t));
  tmp->thread_id = 0;
  tmp->thread_priority = 0;
  tmp->thread_context = (ucontext_t *) calloc(1,sizeof(ucontext_t));
  if (getcontext(tmp->thread_context) == -1) {
    perror("getcontext");
    exit(EXIT_FAILURE);
  }
  //Add to running queue
  addQueue(running,tmp);
}

void t_create(void (*fct)(int), int id, int pri)
{
  if(ready != NULL){
    tcb_t *tmp = (tcb_t *) calloc(1,sizeof(tcb_t));
    tmp->thread_id = id;
    tmp->thread_priority = pri;
    tmp->thread_context = (ucontext_t *) calloc(1,sizeof(ucontext_t));

    if (getcontext(tmp->thread_context) == -1) {
      perror("getcontext");
      exit(EXIT_FAILURE);
    }

    size_t sz = 0x10000;
    tmp->thread_context->uc_stack.ss_sp = calloc(1,sz);
    //uc->uc_stack.ss_sp = mmap(0, sz,
    //     PROT_READ | PROT_WRITE | PROT_EXEC,
    //     MAP_PRIVATE | MAP_ANON, -1, 0);
    tmp->thread_context->uc_stack.ss_size = sz;
    tmp->thread_context->uc_stack.ss_flags = 0;
    tmp->thread_context->uc_link = running->head->thread_context; 
    makecontext(tmp->thread_context, (void (*)()) fct, 1, id);
    
    addQueue(ready,tmp);
  }
}

void t_yield()
{
  if(ready != NULL && ready->head != NULL){
    tcb_t *tmp = rmQueue(running);
    addQueue(running,rmQueue(ready));
    addQueue(ready,tmp);
    swapcontext(tmp->thread_context, running->head->thread_context);
  }
}

void t_terminate(){
  if(running != NULL && ready != NULL){
    tcb_t *tmp = rmQueue(running);
    //printf("terminating id:%d\n",tmp->thread_id);
    free(tmp->thread_context->uc_stack.ss_sp);
    free(tmp->thread_context);
    free(tmp);

    addQueue(running,rmQueue(ready));
    setcontext(running->head->thread_context);
  }
}

void t_shutdown(){
  tcb_t *iter = ready->head;
  while(iter != NULL){
    tcb_t *tmp = iter;
    iter = iter->next;
   // printf("freeing ready id:%d\n",tmp->thread_id);
    free(tmp->thread_context->uc_stack.ss_sp);
    free(tmp->thread_context);
    free(tmp);
  }
  
  iter = running->head;
  while(iter != NULL){
    tcb_t *tmp = iter;
    iter = iter->next;
    //printf("freeing running id:%d\n",tmp->thread_id);
    if(tmp->thread_id > 0){
      free(tmp->thread_context->uc_stack.ss_sp);
    }
    free(tmp->thread_context);
    free(tmp);
  }

  free(ready);
  free(running);
  ready = NULL;
  running = NULL;
}

tQueue_t* createQueue(){
  tQueue_t *tmp = (tQueue_t *) calloc(1,sizeof(tQueue_t));
  tmp->head = tmp->tail = NULL;
  return tmp;
}

void addQueue(tQueue_t *q, tcb_t *t){
  if(q->tail == NULL){
    q->head = q->tail =  t;
    q->tail->next = NULL;
  }
  else{
    q->tail->next = t;
    q->tail = t;
    q->tail->next = NULL;
  }
}

tcb_t* rmQueue(tQueue_t *q){
  if(q->head == NULL){
    return NULL;
  }
  tcb_t* tmp = q->head;
  q->head = q->head->next;
  if(q->head == NULL){
    q->tail = NULL;
  }
  return tmp;
}

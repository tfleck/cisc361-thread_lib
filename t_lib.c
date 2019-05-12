#include "t_lib.h"

tQueue_t *running;
tQueue_t *ready_high;
tQueue_t *ready_low;

int timeout = 10000;

void t_init() {
  //Ignore alarms
  sighold(SIGALRM);

  //Initialize queues
  running = createQueue();
  ready_high = createQueue();
  ready_low = createQueue();

  //Create TCB for main thread
  tcb_t *tmp = (tcb_t *) calloc(1,sizeof(tcb_t));
  tmp->thread_id = 0;
  tmp->thread_priority = 1;
  tmp->thread_context = (ucontext_t *) calloc(1,sizeof(ucontext_t));
  if (getcontext(tmp->thread_context) == -1) {
    perror("getcontext");
    exit(EXIT_FAILURE);
  }

  //Add to running queue
  addQueue(running,tmp);
  
  //Start scheduling alarms
  init_alarm();
  
  sigrelse(SIGALRM);
}

void t_create(void (*fct)(int), int id, int pri) {
  if(ready_high != NULL && ready_low != NULL){
    //Allocate space for new thread
    tcb_t *tmp = (tcb_t *) calloc(1,sizeof(tcb_t));
    tmp->thread_id = id;
    tmp->thread_priority = pri;
    tmp->thread_context = (ucontext_t *) calloc(1,sizeof(ucontext_t));

    if (getcontext(tmp->thread_context) == -1) {
      perror("getcontext");
      exit(EXIT_FAILURE);
    }

    //Create thread context
    size_t sz = 0x2000; //Errors with larger sizes
    tmp->thread_context->uc_stack.ss_sp = calloc(1,sz);
    //uc->uc_stack.ss_sp = mmap(0, sz,
    //     PROT_READ | PROT_WRITE | PROT_EXEC,
    //     MAP_PRIVATE | MAP_ANON, -1, 0);
    tmp->thread_context->uc_stack.ss_size = sz;
    tmp->thread_context->uc_stack.ss_flags = 0;
    tmp->thread_context->uc_link = running->head->thread_context; 
    makecontext(tmp->thread_context, (void (*)()) fct, 1, id);
    
    //Queue thread according to priority
    if(pri == 0){
      addQueue(ready_high,tmp);
    }
    else{
      addQueue(ready_low,tmp);
    }
  }
}

void t_yield() {
  //Ignore alarms
  sighold(SIGALRM);
  
  if(running != NULL && ready_high != NULL && ready_low != NULL && (ready_high->head != NULL || ready_low->head != NULL)){
    //Cancel alarm
    ualarm(0,0);
    
    //Move next ready thread into running queue
    tcb_t *tmp = rmQueue(running);
    if(ready_high->head != NULL){
      addQueue(running,rmQueue(ready_high));
    }
    else{
      addQueue(running,rmQueue(ready_low));
    }
    
    //Move running thread into ready queue
    if(tmp->thread_priority == 0){
      addQueue(ready_high,tmp);
    }
    else{
      addQueue(ready_low,tmp);
    }
    if(running->head == NULL){
      perror("ready empty");
      exit(EXIT_FAILURE);
    }
    
    //Set scheduling alarm, and switch to new running thread
    ualarm(timeout,0);
    swapcontext(tmp->thread_context, running->head->thread_context);
  }
  sigrelse(SIGALRM);
}

void t_terminate() {
  //Ignore alarms
  sighold(SIGALRM);
  
  if(running != NULL && ready_high != NULL && ready_low != NULL && (ready_high->head != NULL || ready_low->head != NULL)){
    //Cancel alarm
    ualarm(0,0);
    
    //Erase currently running thread
    tcb_t *tmp = rmQueue(running);
    free(tmp->thread_context->uc_stack.ss_sp);
    free(tmp->thread_context);
    free(tmp);

    //Queue next thread from ready queue
    if(ready_high->head != NULL){
      addQueue(running,rmQueue(ready_high));
    }
    else{
      addQueue(running,rmQueue(ready_low));
    }
    if(running->head == NULL){
      perror("ready empty");
      exit(EXIT_FAILURE);
    }
    
    //Set scheduling alarm, and switch to new running thread
    ualarm(timeout,0);
    setcontext(running->head->thread_context);
  }
  sigrelse(SIGALRM);
}

void t_shutdown() {
  //Ignore timer
  sighold(SIGALRM);
  
  //Erase all queues, and threads inside
  if(ready_low != NULL){
    tcb_t *iter = ready_low->head;
    while(iter != NULL){
      tcb_t *tmp = iter;
      iter = iter->next;
      if(tmp->thread_id > 0){
        free(tmp->thread_context->uc_stack.ss_sp);
      }
      free(tmp->thread_context);
      free(tmp);
    }
    free(ready_low);
  }
  if(ready_high != NULL){
    tcb_t *iter = ready_high->head;
    while(iter != NULL){
      tcb_t *tmp = iter;
      iter = iter->next;
      if(tmp->thread_id > 0){
        free(tmp->thread_context->uc_stack.ss_sp);
      }
      free(tmp->thread_context);
      free(tmp);
    }
    free(ready_high);
  }
  if(running != NULL){
    tcb_t *iter = running->head;
    while(iter != NULL){
      tcb_t *tmp = iter;
      iter = iter->next;
      if(tmp->thread_id > 0){
        free(tmp->thread_context->uc_stack.ss_sp);
      }
      free(tmp->thread_context);
      free(tmp);
    }
    free(running);
  }
  
  //Set queues to null, so fns can tell not initialized
  ready_low = NULL;
  ready_high = NULL;
  running = NULL;
  sigrelse(SIGALRM);
}

void sem_init(sem_t **sp, int sem_count) {
  //Ignore timer
  sighold(SIGALRM);
  
  //Allocate new semaphore with provided count
  *sp = calloc(1,sizeof(sem_t));
  (*sp)->count = sem_count;
  (*sp)->q = createQueue();
  
  sigrelse(SIGALRM);
}

void sem_wait(sem_t *sp) {
 //Ignore timer
  sighold(SIGALRM);

  sp->count--;
  
  //Block current thread and switch if counter goes negative
  if(sp->count < 0){
    if(running != NULL && ready_high != NULL && ready_low != NULL && (ready_high->head != NULL || ready_low->head != NULL)){
      //Cancel alarm
      ualarm(0,0);
      
      //Move next ready thread into running queue
      tcb_t *tmp = rmQueue(running);
      addQueue(sp->q,tmp);
      if(ready_high->head != NULL){
        addQueue(running,rmQueue(ready_high));
      }
      else{
        addQueue(running,rmQueue(ready_low));
      }
      
      //Set scheduling alarm, and switch to new running thread
      ualarm(timeout,0);
      swapcontext(tmp->thread_context, running->head->thread_context);
    }    
  }
  sigrelse(SIGALRM);
}

void sem_signal(sem_t *sp) {
  //Ignore timer
  sighold(SIGALRM);
  
  sp->count++;
  
  //Move thread out of semaphore queue back into ready queues if count going positive
  if(sp->count <= 0){
    if(running != NULL && ready_high != NULL && ready_low != NULL){
      
      //Move next thread from semaphore queue into ready queue
      tcb_t *tmp = rmQueue(sp->q);
      if(tmp->thread_priority == 0){
        addQueue(ready_high,tmp);
      }
      else{
        addQueue(ready_low,tmp);
      }
    }
  }
  sigrelse(SIGALRM);
}

void sem_destroy(sem_t **sp){
  //Ignore timer
  sighold(SIGALRM);
  
  //Move all threads waiting on semaphore into ready queues
  tcb_t *iter = (*sp)->q->head;
  while(iter != NULL){
    tcb_t *tmp = iter;
    iter = iter->next;
    if(tmp->thread_priority == 0){
      addQueue(ready_high,tmp);
    }
    else{
      addQueue(ready_low,tmp);
    }
  }
  
  //Free semaphore memory allocations
  free((*sp)->q);
  free(*sp);
  
  sigrelse(SIGALRM);  
}

void sig_handler() {
  //If SIGALRM received, force current running thread to yield
  t_yield();
}

void init_alarm() {
  //Start scheduling alarms on timeout interval
  sigset(SIGALRM, sig_handler);
  ualarm(timeout,0);
}

tQueue_t* createQueue() {
  //Allocate space for new Queue
  tQueue_t *tmp = (tQueue_t *) calloc(1,sizeof(tQueue_t));
  tmp->head = tmp->tail = NULL;
  return tmp;
}

void addQueue(tQueue_t *q, tcb_t *t) {
  //If queue empty, head = tail
  if(q->tail == NULL){
    q->head = q->tail =  t;
    q->tail->next = NULL;
  }
  
  //Otherwise, append to list
  else{
    q->tail->next = t;
    q->tail = t;
    q->tail->next = NULL;
  }
}

tcb_t* rmQueue(tQueue_t *q) {
  //If list empty, do nothing
  if(q->head == NULL){
    return NULL;
  }
  
  //Pop node off the front of the queue, and return it
  tcb_t* tmp = q->head;
  q->head = q->head->next;
  if(q->head == NULL){
    q->tail = NULL;
  }
  
  return tmp;
}

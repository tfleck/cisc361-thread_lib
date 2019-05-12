#include "t_lib.h"

tQueue_t *running;
tQueue_t *ready_high;
tQueue_t *ready_low;

int timeout = 10000;

void t_init() {
  sighold(SIGALRM);
  init_alarm();

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
  sigrelse(SIGALRM);
}

void t_create(void (*fct)(int), int id, int pri) {
  if(ready_high != NULL && ready_low != NULL){
    tcb_t *tmp = (tcb_t *) calloc(1,sizeof(tcb_t));
    tmp->thread_id = id;
    tmp->thread_priority = pri;
    tmp->thread_context = (ucontext_t *) calloc(1,sizeof(ucontext_t));

    if (getcontext(tmp->thread_context) == -1) {
      perror("getcontext");
      exit(EXIT_FAILURE);
    }

    size_t sz = 0x2000;
    tmp->thread_context->uc_stack.ss_sp = calloc(1,sz);
    //uc->uc_stack.ss_sp = mmap(0, sz,
    //     PROT_READ | PROT_WRITE | PROT_EXEC,
    //     MAP_PRIVATE | MAP_ANON, -1, 0);
    tmp->thread_context->uc_stack.ss_size = sz;
    tmp->thread_context->uc_stack.ss_flags = 0;
    tmp->thread_context->uc_link = running->head->thread_context; 
    makecontext(tmp->thread_context, (void (*)()) fct, 1, id);
    
    if(pri == 0){
      addQueue(ready_high,tmp);
    }
    else{
      addQueue(ready_low,tmp);
    }
  }
}

void t_yield() {
  sighold(SIGALRM);
  if(running != NULL && ready_high != NULL && ready_low != NULL && (ready_high->head != NULL || ready_low->head != NULL)){
    ualarm(0,0);
    tcb_t *tmp = rmQueue(running);
    if(ready_high->head != NULL){
      addQueue(running,rmQueue(ready_high));
    }
    else{
      addQueue(running,rmQueue(ready_low));
    }
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
    ualarm(timeout,0);
    swapcontext(tmp->thread_context, running->head->thread_context);
  }
  sigrelse(SIGALRM);
}

void t_terminate() {
  sighold(SIGALRM);
  if(running != NULL && ready_high != NULL && ready_low != NULL && (ready_high->head != NULL || ready_low->head != NULL)){
    ualarm(0,0);
    tcb_t *tmp = rmQueue(running);
    free(tmp->thread_context->uc_stack.ss_sp);
    free(tmp->thread_context);
    free(tmp);

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
    ualarm(timeout,0);
    setcontext(running->head->thread_context);
  }
  sigrelse(SIGALRM);
}

void t_shutdown() {
  sighold(SIGALRM);
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
  ready_low = NULL;
  ready_high = NULL;
  running = NULL;
  sigrelse(SIGALRM);
}

void sem_init(sem_t **sp, int sem_count) {
    *sp = calloc(1,sizeof(sem_t));
    (*sp)->count = sem_count;
    (*sp)->q = createQueue();
}

void sem_wait(sem_t *sp) {
 //Ignore timer
  sighold(SIGALRM);

  sp->count --;
  if(sp->count < 0){
    if(running != NULL && ready_high != NULL && ready_low != NULL && (ready_high->head != NULL || ready_low->head != NULL)){
      ualarm(0,0);
      tcb_t *tmp = rmQueue(running);
      addQueue(sp->q,tmp);
      if(ready_high->head != NULL){
        addQueue(running,rmQueue(ready_high));
      }
      else{
        addQueue(running,rmQueue(ready_low));
      }
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
  if(sp->count <= 0){
    if(running != NULL && ready_high != NULL && ready_low != NULL){
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
  free((*sp)->q);
  free(*sp);
  sigrelse(SIGALRM);  
}

void sig_handler() {
  t_yield();
}

void init_alarm() {
  sigset(SIGALRM, sig_handler);
  ualarm(timeout,0);
}

tQueue_t* createQueue() {
  tQueue_t *tmp = (tQueue_t *) calloc(1,sizeof(tQueue_t));
  tmp->head = tmp->tail = NULL;
  return tmp;
}

void addQueue(tQueue_t *q, tcb_t *t) {
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

tcb_t* rmQueue(tQueue_t *q) {
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

#include "t_lib.h"

tQueue_t *running;
tQueue_t *ready_high;
tQueue_t *ready_low;
tQueue_t *all;

int timeout = 10000;

void t_init() {
  //Ignore alarms
  sighold(SIGALRM);

  //Initialize queues
  running = createQueue();
  ready_high = createQueue();
  ready_low = createQueue();
  all = createQueue();

  //Create TCB for main thread
  tcb_t *tmp = (tcb_t *) calloc(1,sizeof(tcb_t));
  tmp->thread_id = -1;
  tmp->thread_priority = 1;
  mbox_create(&(tmp->mail));
  tmp->thread_context = (ucontext_t *) calloc(1,sizeof(ucontext_t));
  if (getcontext(tmp->thread_context) == -1) {
    perror("getcontext");
    exit(EXIT_FAILURE);
  }

  //Add to running queue
  addQueue(running,tmp);
  addQueue(all,tmp);
  
  //Start scheduling alarms
  init_alarm();
  
  sigrelse(SIGALRM);
}

void t_create(void (*fct)(int), int id, int pri) {
  if(ready_high != NULL && ready_low != NULL && all != NULL){
    //Allocate space for new thread
    tcb_t *tmp = (tcb_t *) calloc(1,sizeof(tcb_t));
    if(id < 0){
      perror("ID must be > 0");
      exit(EXIT_FAILURE);
    }
    if(findById(all,id) != NULL){
      perror("ID must be unique");
      exit(EXIT_FAILURE);
    }
    tmp->thread_id = id;
    tmp->thread_priority = pri;
    mbox_create(&(tmp->mail));
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
    addQueue(all,tmp);
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
    tcb_t *tmp = rmQueue(running,-1);
    if(ready_high->head != NULL){
      addQueue(running,rmQueue(ready_high,-1));
    }
    else{
      addQueue(running,rmQueue(ready_low,-1));
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
    
    //Pop current running thread
    tcb_t *tmp = rmQueue(running,-1);

    //Queue next thread from ready queue
    if(ready_high->head != NULL){
      addQueue(running,rmQueue(ready_high,-1));
    }
    else{
      addQueue(running,rmQueue(ready_low,-1));
    }
    if(running->head == NULL){
      perror("ready empty, no thread to run");
      exit(EXIT_FAILURE);
    }
    
    //Erase currently running thread
    rmQueue(all,tmp->thread_id);
    mbox_destroy(&(tmp->mail));
    free(tmp->thread_context->uc_stack.ss_sp);
    free(tmp->thread_context);
    free(tmp);
    
    //Set scheduling alarm, and switch to new running thread
    ualarm(timeout,0);
    setcontext(running->head->thread_context);
  }
  sigrelse(SIGALRM);
}

void t_shutdown() {
  //Ignore timer
  sighold(SIGALRM);
  
  if(all != NULL){
    tcb_t *iter = all->head;
    while(iter != NULL){
      tcb_t *tmp = iter;
      iter = iter->next_all;
      mbox_destroy(&(tmp->mail));
      free(tmp->thread_context->uc_stack.ss_sp);
      free(tmp->thread_context);
      free(tmp);
    }
    free(all);
  }
  if(running != NULL){
    free(running);
  }
  if(ready_high != NULL){
    free(ready_high);
  }
  if(ready_low != NULL){
    free(ready_low);
  }
  
  //Set queues to null, so fns can tell not initialized
  ready_low = NULL;
  ready_high = NULL;
  running = NULL;
  all = NULL;
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
      tcb_t *tmp = rmQueue(running,-1);
      addQueue(sp->q,tmp);
      if(ready_high->head != NULL){
        addQueue(running,rmQueue(ready_high,-1));
      }
      else{
        addQueue(running,rmQueue(ready_low,-1));
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
      tcb_t *tmp = rmQueue(sp->q,-1);
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

void mbox_create(mbox **mb){
  //Ignore timer
  sighold(SIGALRM);
  
  //Allocate space for new mbox
  mbox *new_mbox = (mbox *) calloc(1,sizeof(mbox));
  new_mbox->msg = NULL;
  sem_init(&(new_mbox->mbox_send),1); //Allow sending
  sem_init(&(new_mbox->mbox_recv),0); //No messages yet
  *mb = new_mbox;
  
  sigrelse(SIGALRM);
}

void mbox_destroy(mbox **mb){
  //Ignore timer
  sighold(SIGALRM);
  
  //Loop over messages and destroy all
  messageNode *tmp = (*mb)->msg;
  while(tmp != NULL){
    messageNode *tmp2 = tmp;
    tmp = tmp->next;
    sem_signal(tmp2->recv_wait);
    sem_destroy(&(tmp2->recv_wait));
    free(tmp2->message);
    free(tmp2);
  }
  
  //Destroy semaphores and mailbox
  sem_destroy(&((*mb)->mbox_send));
  sem_destroy(&((*mb)->mbox_recv));
  free(*mb);
  
  sigrelse(SIGALRM);
}

void mbox_deposit(mbox *mb, char *msg, int len){
  //Ignore timer
  sighold(SIGALRM);
  
  //Allocate new messageNode
  messageNode *new_msg = calloc(1,sizeof(messageNode));
  new_msg->message = calloc(len+1,sizeof(char));
  strncpy(new_msg->message, msg, len+1);
  new_msg->len = len+1;
  new_msg->sender = running->head->thread_id;
  sem_init(&(new_msg->recv_wait),0);
  new_msg->next = NULL;
  
  //Acquire lock on mailbox sending
  sem_wait(mb->mbox_send);
  
  //Append message to mailbox
  if(mb->msg == NULL){
    mb->msg = new_msg;
  }
  else{
    messageNode *head_msg = mb->msg;
    while(head_msg->next != NULL){
      head_msg = head_msg->next;
    }
    head_msg->next = new_msg;
  }
  
  //Release mailbox sending lock
  sem_signal(mb->mbox_send);
  
  //Increase count of messages to be received
  sem_signal(mb->mbox_recv);
  
  sigrelse(SIGALRM);
}

void mbox_withdraw(mbox *mb, char *msg, int *len){
  //Ignore timer
  sighold(SIGALRM);
  
  //Get first message in mailbox
  messageNode *head_msg = mb->msg;
  
  if(head_msg == NULL){
    //Mailbox is empty
    *len = 0;
  }
  else{
    //Transfer message attributes to arguments
    strncpy(msg,head_msg->message,head_msg->len);
    *len = head_msg->len-1;
    
    //Remove message from list and destroy it
    mb->msg = head_msg->next;
    sem_signal(head_msg->recv_wait);
    sem_destroy(&(head_msg->recv_wait));
    free(head_msg->message);
    free(head_msg);
  }
  sigrelse(SIGALRM);
}

void send(int tid, char *msg, int len){
  //Ignore timer
  sighold(SIGALRM);
  
  //Find TCB of thread to send to
  tcb_t *tmp = findById(all,tid);
  if(tmp == NULL){
    sigrelse(SIGALRM);
    return;
  }
  
  //Allocate new messageNode
  messageNode *new_msg = calloc(1,sizeof(messageNode));
  new_msg->message = calloc(len+1,sizeof(char));
  strncpy(new_msg->message, msg, len+1);
  new_msg->len = len+1;
  new_msg->sender = running->head->thread_id;
  new_msg->receiver = tid;
  sem_init(&(new_msg->recv_wait),0);
  
  //Acquire lock on mailbox sending
  sem_wait(tmp->mail->mbox_send);
  
  //Append message to mailbox
  if(tmp->mail->msg == NULL){
    tmp->mail->msg = new_msg;
  }
  else{
    messageNode *head_msg = tmp->mail->msg;
    while(head_msg->next != NULL){
      head_msg = head_msg->next;
    }
    head_msg->next = new_msg;
  }
  
  //Release mailbox sending lock
  sem_signal(tmp->mail->mbox_send);
  
  //Increase count of messages to be received
  sem_signal(tmp->mail->mbox_recv);
  
  sigrelse(SIGALRM);
}

void receive(int *tid, char *msg, int *len){
  //Ignore timer
  sighold(SIGALRM);
  
  //Wait for number of messages to be non-zero
  sem_wait(running->head->mail->mbox_recv);
  
  //Loop over messages looking for a TID match
  messageNode *tmp_msg = running->head->mail->msg;
  if(tmp_msg == NULL){
    //Shouldn't happen
    *len = 0;
  }
  else{
    //Prevent sending while receiving
    sem_wait(running->head->mail->mbox_send);
    
    //Special case for first message in the list
    if(*tid == 0 || *tid == tmp_msg->sender){
      //Transfer message attributes to arguments
      strncpy(msg,tmp_msg->message,tmp_msg->len);
      *len = tmp_msg->len-1;
      *tid = tmp_msg->sender;
      
      //Remove message from list and destroy it
      messageNode *tmp_msg2 = tmp_msg;
      running->head->mail->msg = tmp_msg->next;
      sem_signal(tmp_msg2->recv_wait);
      sem_destroy(&(tmp_msg2->recv_wait));
      free(tmp_msg2->message);
      free(tmp_msg2);
    }
    else{
      //Loop over messages until TID match found
      while(tmp_msg->next != NULL){
        if(*tid == 0 || *tid == tmp_msg->next->sender){
          //Transfer message attributes to arguments
          strncpy(msg,tmp_msg->next->message,tmp_msg->next->len);
          *len = tmp_msg->next->len-1;
          *tid = tmp_msg->sender;
          
          //Remove message from list and destroy it
          messageNode *tmp_msg2 = tmp_msg->next;
          tmp_msg->next = tmp_msg->next->next;
          sem_signal(tmp_msg2->recv_wait);
          sem_destroy(&(tmp_msg2->recv_wait));
          free(tmp_msg2->message);
          free(tmp_msg2);
          break;
        }
        tmp_msg = tmp_msg->next;
      }
    }
    //Allow sending to mailbox again
    sem_signal(running->head->mail->mbox_send);
  }
  //Decrement count of messages to be received
  sem_signal(running->head->mail->mbox_recv);
  
  sigrelse(SIGALRM);
}

void block_send(int tid, char *msg, int len){
  //Ignore timer
  sighold(SIGALRM);
  
  //Find TCB of thread to send to
  tcb_t *tmp = findById(all,tid);
  if(tmp == NULL){
    sigrelse(SIGALRM);
    return;
  }
  
  //Allocate new messageNode
  messageNode *new_msg = calloc(1,sizeof(messageNode));
  new_msg->message = calloc(len+1,sizeof(char));
  strncpy(new_msg->message, msg, len+1);
  new_msg->len = len+1;
  new_msg->sender = running->head->thread_id;
  new_msg->receiver = tid;
  sem_init(&(new_msg->recv_wait),0);
  
  //Acquire lock on mailbox sending
  sem_wait(tmp->mail->mbox_send);
  
  //Append message to mailbox
  if(tmp->mail->msg == NULL){
    tmp->mail->msg = new_msg;
  }
  else{
    messageNode *head_msg = tmp->mail->msg;
    while(head_msg->next != NULL){
      head_msg = head_msg->next;
    }
    head_msg->next = new_msg;
  }
  
  //Release mailbox sending lock
  sem_signal(tmp->mail->mbox_send);
  
  //Increase count of messages to be received
  sem_signal(tmp->mail->mbox_recv);
  
  //Wait for message to be received or destroyed
  sem_wait(new_msg->recv_wait);
  
  sigrelse(SIGALRM);
}

void block_receive(int *tid, char *msg, int *len){
  //standard receive already blocks
  receive(tid,msg,len);
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
  //Special case for all queue
  if(q == all){
    //If queue empty, head = tail
    if(q->tail == NULL){
      q->head = q->tail =  t;
      q->tail->next_all = NULL;
    }
    //Otherwise, append to list
    else{
      q->tail->next_all = t;
      q->tail = t;
      q->tail->next_all = NULL;
    }
  }
  else {
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
}

tcb_t* rmQueue(tQueue_t *q, int tid) {
  //Special case for all queue
  if(q == all){
    //If list empty, do nothing
    if(q->head == NULL){
      return NULL;
    }
    
    //If head node matches TID, remove and return it
    if(tid == -1 || q->head->thread_id == tid){
      //Pop node off the front of the queue, and return it
      tcb_t *tmp = q->head;
      q->head = q->head->next_all;
      if(q->head == NULL){
        q->tail = NULL;
      }
      return tmp;
    }
    else{
      //Loop through queue to match TID, remove and return it
      tcb_t* tmp = q->head;
      while(tmp->next_all != NULL){
        if(tmp->next_all->thread_id == tid){
          tcb_t *tmp2 = tmp->next_all;
          if(q->tail == tmp2){
            //If at the end, set next->next to null
            q->tail = tmp;
            tmp->next_all = NULL;
          }
          else{
            //If not at the end, skip node being removed
            tmp->next_all = tmp->next_all->next_all;
          }
          return tmp2;
        }
        tmp = tmp->next_all;
      }
    }
  }
  
  //All other queues
  else {
    //If list empty, do nothing
    if(q->head == NULL){
      return NULL;
    }
    
    //If head node matches TID, remove and return it
    if(tid == -1 || q->head->thread_id == tid){
      //Pop node off the front of the queue, and return it
      tcb_t *tmp = q->head;
      q->head = q->head->next;
      if(q->head == NULL){
        q->tail = NULL;
      }
      return tmp;
    }
    else{
      //Loop through queue to match TID and return it
      tcb_t* tmp = q->head;
      while(tmp->next != NULL){
        if(tmp->next->thread_id == tid){
          tcb_t *tmp2 = tmp->next;
          if(q->tail == tmp2){
            //If at the end, set next->next to null
            q->tail = tmp;
            tmp->next = NULL;
          }
          else{
            //If not at the end, skip node being removed
            tmp->next = tmp->next->next;
          }
          return tmp2;
        }
        tmp = tmp->next;
      }
    }
  }
  
  //If matching node not found, do nothing
  return NULL;
}

tcb_t* findById(tQueue_t *q, int tid){
  //Special case for all queue
  if(q == all){
    //If list empty, do nothing
    if(q->head == NULL){
      return NULL;
    }
    
    //Loop over TCBs looking for a TID match to return
    tcb_t *tmp = q->head;
    while(tmp != NULL){
      
      if(tmp->thread_id == tid){
        return tmp;
      }
      tmp = tmp->next_all;
    }
  }
  else {
    //If list empty, do nothing
    if(q->head == NULL){
      return NULL;
    }
    
    //Loop over TCBs looking for a TID match to return
    tcb_t *tmp = q->head;
    while(tmp != NULL){
      if(tmp->thread_id == tid){
        return tmp;
      }
      tmp = tmp->next;
    }
  }
  
  //If nothing found, return null
  return NULL;
}
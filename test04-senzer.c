/*Phase 4 Mailbox Function Test
Zachary Senzer & Matthew Spicer */

#include <stdio.h>
#include <stdlib.h>
#include "ud_thread.h"

mbox *mb;
char *msg[2] = {"Message 1", "Another Message"};

void producer(int id)
{
  int i;
  char mymsg[30];

  for (i = 0; i < 2; i++) {
    sprintf(mymsg, "%s - tid %d", msg[i], id);
    printf("Created MSG: (%d): [%s] [length=%d]\n", id, mymsg, strlen(mymsg));
    fflush(stdout);
    mbox_deposit(mb, mymsg, strlen(mymsg));
  }

  t_terminate();
}

void consumer(int id)
{
  int i;
  int len;
  char mesg[1024];

  for (i = 0; i < 4; i++) {
    mbox_withdraw(mb, mesg, &len);
    printf("MBox Message: [%s], length=%d\n", mesg, len);
    fflush(stdout);
  }

  t_terminate();
}

int main(void) {

   t_init();

   mbox_create(&mb);
   t_create(producer, 1, 1);
   t_create(producer, 2, 1);
   t_create(consumer, 3, 1);
   t_yield();

   int len;
   char mesg[1024];
   mbox_withdraw(mb, mesg, &len); // should print a warning about the mailbox not having any messages

   mbox_destroy(&mb);

   t_shutdown();
   printf("Done with mailbox test...\n");
   fflush(stdout);

   return 0;
}

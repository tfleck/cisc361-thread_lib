/*
 * Test Program #7 - Testing 2LQ
 */

#include <stdio.h>
#include "ud_thread.h"

void thread_low(int val) {

   int i;

   for(i = 0; i < 10; i++) {

      printf("I am thread %d...\n", val);
      t_yield();
   }

   t_terminate();
}

void thread_another(int val) {

   int i;

   printf("Created thread_another!\n");

   for(i = 0; i < 4; i++) {

      printf("I am thread %d...\n", val);
      t_yield();
   }

   t_terminate();
}

void thread_high(int val) {

   int i;

   printf("Creating thread 3...\n");
   t_create(thread_another, 3, 0);
   printf("DONE Creating thread 3...\n");

   for(i = 0; i < 4; i++) {

      printf("I am thread %d...\n", val);
      t_yield();
   }

   t_terminate();
}

int main(void) {

   int i;

   t_init();

   t_create(thread_low,  1, 1);
   printf("I am main...\n");
   t_create(thread_high, 2, 0);
 
   for(i = 0; i < 15; i++) {

      printf("I am main...\n");
      t_yield();
   }

   t_shutdown();

   return 0;
}

/*
 * Test Program #4 - Time Slicing
 */

#include <stdio.h>
#include "ud_thread.h"

void thread_function(int val) {

   int i = 0;
   printf("I am thread %d...\n", val);

   if(val < 5) {
     
      t_create(thread_function, val+1, 1);
   }

   for(;;) {

      for(i = 0; i < 10000000; i++);
      printf("I am thread %d...\n", val);
   }
}

int main(void){

   int i;

   t_init();
   t_create(thread_function, 1, 1);
  
   for (;;) {

      for(i = 0; i < 10000000; i++);
      printf("I am main...\n");
   }

   return 0;
}


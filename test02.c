/*
 * Test Program #2 - Time Slicing
 */

#include <stdio.h>
#include "ud_thread.h"

void function(int thr_id) {

   int i, j, k = 1;

   for (i = j = 0; k == k; i++, j++) {

      printf("this is thread %d [%d]...\n", thr_id, j);
   }

   printf("Thread %d is done...\n", thr_id);
   t_terminate();
}

int main(void) {

   int i, k = 1;

   t_init();

   t_create(function, 1, 1);
   printf("This is main()...\n");
   t_create(function, 2, 1);
   printf("This is main()...\n");
   t_create(function, 3, 1);

   for(i = 0; i < 5000; i++) {
      printf("This is main(%d)...\n", i);
   }

   printf("Begin shutdown...\n");
   t_shutdown();
   printf("Done with shutdown...\n");

   return 0;
}


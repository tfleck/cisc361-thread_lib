/*
 * Test Program #2a - Time Slicing
 */

#include <stdio.h>
#include "ud_thread.h"

int main(void) {

   int i, k = 1;

   t_init();

   for(i = 0; i < 5000; i++) {
      printf("This is main (%d)...\n", i);
   }

   t_shutdown();

   return 0;
}


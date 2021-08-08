// Example plugin for webc webserver
//

#include <stdbool.h>
#include <stdio.h>

bool webc_init (void)
{
   printf ("plugin loaded\n");
   return true;
}

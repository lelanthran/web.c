#include <stdio.h>

#include "web-main.h"
#include "web-add.h"

int main (int argc, char **argv)
{
   argc = argc;
   argv = argv;
   printf ("Starting the web.c server\n");

   return web_main ();
}


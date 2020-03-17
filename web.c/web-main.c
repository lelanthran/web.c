#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>

#include <sys/socket.h>
#include <unistd.h>

#include "web-main.h"
#include "web-add.h"
#include "util.h"
#include "config.h"
#include "resource.h"
#include "handler.h"

static char *g_portnum = NULL;
static volatile sig_atomic_t g_exit_program = 0;
static size_t g_timeout = TIMEOUT_TO_SHUTDOWN;

static void signal_handler (int n);

int main (int argc, char **argv)
{
   int ret = EXIT_FAILURE;

   uint32_t portnum = DEFAULT_LISTEN_PORT;
   int backlog = DEFAULT_BACKLOG;
   int listenfd = -1;

   int clientfd = -1;
   char *remote_addr = NULL;
   uint16_t remote_port = 0;

   argc = argc;
   argv = argv;

   if (!(g_portnum = strdup ("5998"))) {
      goto errorexit;
   }

   if ((chdir (DEFAULT_WEB_ROOT))!=0) {
      UTIL_LOG ("Failed to switch to web-root [%s]: %m\n", DEFAULT_WEB_ROOT);
      goto errorexit;
   }

   UTIL_LOG ("Starting the web.c server\n");

   /* ************************************************************** */

   if ((sscanf (g_portnum, "%u", &portnum))!=1) {
      UTIL_LOG ("Listening port [%s] is invalid\n", g_portnum);
      goto errorexit;
   }
   if (portnum >> 16 || portnum==0) {
      UTIL_LOG ("Listening port [%u] is too large\n", portnum);
      goto errorexit;
   }

   /* ************************************************************** */

   if ((signal (SIGINT, signal_handler))==SIG_ERR) {
      UTIL_LOG ("Failed to install signal handler: %m\n");
      goto errorexit;
   }

   /* ************************************************************** */

   if (!(resource_global_handler_lock())) {
      UTIL_LOG ("Failed to aquire global resource handler lock\n");
      goto errorexit;
   }

   if (!(resource_global_handler_add (EXTENSION_HTML, handler_static_file))) {
      UTIL_LOG ("Failed to add handler [%s]\n", EXTENSION_HTML);
      goto errorexit;
   }

   if (!(resource_global_handler_unlock())) {
      UTIL_LOG ("Failed to release global resource handler lock\n");
      goto errorexit;
   }

   /* ************************************************************** */

   if ((listenfd = create_listener (portnum, backlog)) < 0) {
      UTIL_LOG ("Unable to create a listener, aborting\n");
      goto errorexit;
   }

   UTIL_LOG ("Listening on %u q/%i\n", portnum, backlog);

   while (!g_exit_program) {
      free (remote_addr);
      remote_addr = NULL;
      clientfd = accept_conn (listenfd, g_timeout,
                                        &remote_addr,
                                        &remote_port);
      if (clientfd == 0) { // Timeout
         continue;
      }
      if (clientfd < 0) { // Error
         UTIL_LOG ("Failed to accept(), aborting\n");
         goto errorexit;
      }

      UTIL_LOG ("Got client fd %i\n", clientfd);

      if (!(start_webserver (clientfd, remote_addr, remote_port))) {
         UTIL_LOG ("Failed to start webserver for client [%s:%u]\n",
                     remote_addr, remote_port);
         goto errorexit;
      }
      clientfd = -1;
   }

   ret = web_main ();

errorexit:

   free (g_portnum);

   if (listenfd >= 0) {
      shutdown (listenfd, SHUT_RDWR);
      close (listenfd);
   }

   if (clientfd >= 0) {
      shutdown (listenfd, SHUT_RDWR);
      close (clientfd);
   }

   free (remote_addr);

   return ret;
}


static void signal_handler (int n)
{
   switch (n) {
      case SIGINT:   g_exit_program = 1;
   }
}


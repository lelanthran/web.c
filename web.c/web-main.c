#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>


#include "web-main.h"
#include "web-add.h"
#include "util.h"
#include "config.h"

/* Should really define __GNU_SOURCE or set the correct -std flag instead of
 * doing this.
 */
int accept4(int sockfd, struct sockaddr *addr,
            socklen_t *addrlen, int flags);

static char *g_portnum = NULL;
static volatile sig_atomic_t g_exit_program = 0;
static size_t g_timeout = 1;

static void signal_handler (int n);
static int create_listener (uint32_t portnum, int backlog);
static int accept_conn (int listenfd, size_t timeout,
                                      char **remote_addr,
                                      uint16_t *remote_port);
static bool start_webserver (int fd, char *remote_addr, uint16_t remote_port);


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


int create_listener (uint32_t portnum, int backlog)
{
   struct sockaddr_in addr;

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = INADDR_ANY;
   addr.sin_port = htons (portnum & 0xffff);
   int fd = -1;

   fd = socket (AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
   if (fd < 0) {
      UTIL_LOG ("socket() failed: %m\n");
      return -1;
   }

   if (bind (fd, (struct sockaddr *)&addr, sizeof addr)!=0) {
      UTIL_LOG ("bind() failed: %m\n");
      close (fd);
      return -1;
   }
   if (listen (fd, backlog)!=0) {
      UTIL_LOG ("listen() failed: %m\n");
      close (fd);
      return -1;
   }
   return fd;
}


int accept_conn (int listenfd, size_t timeout,
                               char **remote_addr,
                               uint16_t *remote_port)
{
   struct sockaddr_in ret;
   socklen_t retlen = sizeof ret;
   int retval = -1;

   memset (&ret, 0xff, sizeof ret);

   fd_set fds;
   struct timeval tv = { (long int)timeout , 0 };
   FD_ZERO (&fds);
   FD_SET (listenfd, &fds);
   int r = select (listenfd + 1, &fds, &fds, &fds, &tv);
   if (r == 0) {
      return 0;
   }
   if (r < 0) {
      return -1;
   }

   retval = accept4 (listenfd, (struct sockaddr *)&ret, &retlen, SOCK_CLOEXEC);
   if (retval <= 0) {
      return -1;
   }

   if (remote_addr) {
      *remote_addr = malloc (16);
      if (!*remote_addr) {
         return retval;
      }

      uint8_t bytes[4];
      bytes[3] = (ret.sin_addr.s_addr >> 24) & 0xff;
      bytes[2] = (ret.sin_addr.s_addr >> 16) & 0xff;
      bytes[1] = (ret.sin_addr.s_addr >>  8) & 0xff;
      bytes[0] = (ret.sin_addr.s_addr      ) & 0xff;
      sprintf (*remote_addr, "%u.%u.%u.%u", bytes[0],
                                            bytes[1],
                                            bytes[2],
                                            bytes[3]);
   }

   if (remote_port) {
      *remote_port = ntohs (ret.sin_port);
   }
   return retval;
}


static bool start_webserver (int fd, char *remote_addr, uint16_t remote_port)
{
   UTIL_LOG ("Received connection from [%i:%s:%u]\n", fd,
                                                      remote_addr,
                                                      remote_port);
   close (fd);
   return true;
}



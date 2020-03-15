
#include <string.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include <pthread.h>

#include "util.h"

/* Should really define __GNU_SOURCE or set the correct -std flag instead of
 * doing this.
 */
int accept4(int sockfd, struct sockaddr *addr,
            socklen_t *addrlen, int flags);

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


struct thread_args_t {
   int fd;
   char *remote_addr;
   uint16_t remote_port;
};

static void thread_args_del (struct thread_args_t *args)
{
   if (args) {
      free (args->remote_addr);
      free (args);
   }
}

static struct thread_args_t *thread_args_new (int fd, const char *remote_addr,
                                                      int16_t remote_port)
{
   struct thread_args_t *ret = NULL;

   if (!(ret = calloc (1, sizeof *ret))) {
      return NULL;
   }

   ret->fd = fd;
   ret->remote_addr = strdup (remote_addr);
   ret->remote_port = remote_port;

   if (!ret->remote_addr) {
      thread_args_del (ret);
      ret = NULL;
   }

   return ret;
}


static void *thread_func (void *ta)
{
   struct thread_args_t *args = ta;
   return NULL;
}

bool start_webserver (int fd, char *remote_addr, uint16_t remote_port)
{
   bool error = true;

   pthread_attr_t attr;
   pthread_t thread;
   struct thread_args_t *args = NULL;

   if (!(args = thread_args_new (1, remote_addr, remote_port))) {
      UTIL_LOG ("[%s:%u] OOM error\n", remote_addr, remote_port);
      goto errorexit;
   }

   if ((pthread_attr_init (&attr))!=0) {
      UTIL_LOG ("[%s:%u] Failed to initiliase thread attributes: %m\n",
                  remote_addr, remote_port);
      goto errorexit;
   }
   if ((pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED))!=0) {
      UTIL_LOG ("[%s:%u] Failed to set the thread to detached: %m\n",
                  remote_addr, remote_port);
      goto errorexit;
   }
   if ((pthread_create (&thread, &attr, thread_func, &args))!=0) {
      UTIL_LOG ("[%s:%u] Failed to start thread\n", remote_addr, remote_port);
      goto errorexit;
   }


   UTIL_LOG ("Received connection from [%i:%s:%u]\n", fd,
                                                      remote_addr,
                                                      remote_port);

   error = false;

errorexit:
   if (error) {
      close (fd);
      thread_args_del (args);
   }
   return !error;
}


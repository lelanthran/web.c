#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <inttypes.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>


#include "webc_web-main.h"
#include "webc_web-add.h"
#include "webc_util.h"
#include "webc_config.h"
#include "webc_resource.h"
#include "webc_handler.h"

static volatile sig_atomic_t g_exit_program = 0;
static size_t g_timeout = TIMEOUT_TO_SHUTDOWN;

static void signal_handler (int n);
static const char *read_cline_opt (int argc, char **argv, const char *name);

int main (int argc, char **argv)
{
   int ret = EXIT_FAILURE;

   char *logfile_name = NULL;

   uint32_t portnum = 0;
   int backlog = 0;
   int listenfd = -1;
   uint8_t errcount = 0;

   int clientfd = -1;
   char *remote_addr = NULL;
   uint16_t remote_port = 0;

   /* *************************************************************
    *  Handle the command line arguments
    */
   const char *opt_portnum = read_cline_opt (argc, argv, "port");
   const char *opt_logfile = read_cline_opt (argc, argv, "logfile");
   const char *opt_backlog = read_cline_opt (argc, argv, "backlog");

   bool opt_unknown = false;
   for (size_t i=1; argv[i]; i++) {
      if (argv[i][0]) {
         WEBC_UTIL_LOG ("Unknown option [%s]\n", argv[i]);
         opt_unknown = true;
      }
   }
   if (opt_unknown) {
      WEBC_UTIL_LOG ("Aborting\n");
      return EXIT_FAILURE;
   }

   if (!opt_portnum) {
      opt_portnum = DEFAULT_LISTEN_PORT;
      WEBC_UTIL_LOG ("No port number specified, using default [%s]\n", opt_portnum);
   }

   if (!opt_backlog) {
      opt_backlog = DEFAULT_BACKLOG;
      WEBC_UTIL_LOG ("No backlog specified, using default [%s]\n", opt_backlog);
   }

   if (!opt_logfile) {
      WEBC_UTIL_LOG ("No logfile specified, logging to stderr\n");
   } else {
      int fd_logfile = -1;
      static const char *template = ".YYYYMMDDhhmmss";
      size_t logfile_namelen = 0;
      int fd_stderr = -1;

      time_t now = time (NULL);
      struct tm *time_fields = localtime (&now);
      if (!time_fields) {
         WEBC_UTIL_LOG ("Failed to get local time: %m\n");
         goto errorexit;
      }

      logfile_namelen = strlen (opt_logfile)
                      + strlen (template)
                      + 1;
      if (!(logfile_name = malloc (logfile_namelen))) {
         WEBC_UTIL_LOG ("OOM error constructing logfile name\n");
         goto errorexit;
      }
      snprintf (logfile_name, logfile_namelen, "%s."     // prefix
                                               "%04i"    // YYYY
                                               "%02i"    // MM
                                               "%02i"    // DD
                                               "%02i"    // hh
                                               "%02i"    // mm
                                               "%02i",   // ss
                                               opt_logfile,
                                               time_fields->tm_year + 1900,
                                               time_fields->tm_mon + 1,
                                               time_fields->tm_mday,
                                               time_fields->tm_hour,
                                               time_fields->tm_min,
                                               time_fields->tm_sec);

      fd_logfile = open (logfile_name,
                         O_WRONLY | O_CREAT,
                         S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
      if (fd_logfile < 0) {
         WEBC_UTIL_LOG ("Cannot open logfile [%s]\n", logfile_name);
         goto errorexit;
      }
      if ((fd_stderr = fileno (stderr)) < 0) {
         WEBC_UTIL_LOG ("Unable to get file descriptors for stderr\n");
         close (fd_logfile);
         goto errorexit;
      }
      if ((dup2 (fd_logfile, fd_stderr)) != fd_stderr) {
         WEBC_UTIL_LOG ("Failed to dup() file descriptors for logging: %m\n");
         close (fd_logfile);
         goto errorexit;
      }
      fflush (stdout);
      fflush (stderr);
      WEBC_UTIL_LOG ("Logging to [%s]\n", logfile_name);
      printf ("Logging to [%s]\n", logfile_name);
   }

   if ((chdir (DEFAULT_WEB_ROOT))!=0) {
      WEBC_UTIL_LOG ("Failed to switch to web-root [%s]: %m\n", DEFAULT_WEB_ROOT);
      goto errorexit;
   }

   WEBC_UTIL_LOG ("Starting the web.c server\n");

   /* ************************************************************** */

   if ((sscanf (opt_portnum, "%u", &portnum))!=1) {
      WEBC_UTIL_LOG ("Listening port [%s] is invalid\n", opt_portnum);
      goto errorexit;
   }
   if (portnum >> 16 || portnum==0) {
      WEBC_UTIL_LOG ("Listening port [%u] is too large\n", portnum);
      goto errorexit;
   }

   /* ************************************************************** */

   if ((signal (SIGINT, signal_handler))==SIG_ERR) {
      WEBC_UTIL_LOG ("Failed to install signal handler: %m\n");
      goto errorexit;
   }

   if ((signal (SIGPIPE, SIG_IGN))==SIG_ERR) {
      WEBC_UTIL_LOG ("Failed to block SIGPIPE: %m\n");
      goto errorexit;
   }

   /* ************************************************************** */

   if (!(webc_web_add_init ())) {
      WEBC_UTIL_LOG ("Failed to run the user-supplied initialisation\n");
      goto errorexit;
   }

   if (!(webc_resource_global_handler_lock())) {
      WEBC_UTIL_LOG ("Failed to aquire global resource handler lock\n");
      goto errorexit;
   }

   if (!(webc_resource_global_handler_add ("handler_none",
                                      EXTENSION_NONE, pattern_SUFFIX,
                                      webc_handler_none))) {
      WEBC_UTIL_LOG ("Failed to add handler [%s]\n", EXTENSION_NONE);
      goto errorexit;
   }

   if (!(webc_resource_global_handler_add ("handler_none",
                                      EXTENSION_DIR, pattern_SUFFIX,
                                      webc_handler_dir))) {
      WEBC_UTIL_LOG ("Failed to add handler [%s]\n", EXTENSION_DIR);
      goto errorexit;
   }

   if (!(webc_resource_global_handler_add ("handler_none",
                                      EXTENSION_TEXT, pattern_SUFFIX,
                                      webc_handler_static_file))) {
      WEBC_UTIL_LOG ("Failed to add handler [%s]\n", EXTENSION_TEXT);
      goto errorexit;
   }

   if (!(webc_resource_global_handler_add ("handler_none",
                                      EXTENSION_HTML, pattern_SUFFIX,
                                      webc_handler_html))) {
      WEBC_UTIL_LOG ("Failed to add handler [%s]\n", EXTENSION_HTML);
      goto errorexit;
   }

   if (!(webc_web_add_load_handlers ())) {
      WEBC_UTIL_LOG ("Failed to run the user-supplied load-handlers\n");
      goto errorexit;
   }

   if (!(webc_resource_global_handler_unlock())) {
      WEBC_UTIL_LOG ("Failed to release global resource handler lock\n");
      goto errorexit;
   }

   /* ************************************************************** */

   if ((listenfd = webc_create_listener (portnum, backlog)) < 0) {
      WEBC_UTIL_LOG ("Unable to create a listener, aborting\n");
      goto errorexit;
   }

   WEBC_UTIL_LOG ("Listening on %u q/%i\n", portnum, backlog);

   errcount = 0;
   while (!g_exit_program && errcount < 5) {
      free (remote_addr);
      remote_addr = NULL;
      clientfd = webc_accept_conn (listenfd, g_timeout,
                                            &remote_addr,
                                            &remote_port);
      if (clientfd == 0) { // Timeout
         errcount = 0;
         continue;
      }
      if (clientfd < 0) { // Error
         errcount++;
         WEBC_UTIL_LOG ("Failed to accept(), errcount=%" PRIu8 "\n", errcount);
         continue;
      }

      if (!(webc_handle_conn (clientfd, remote_addr, remote_port))) {
         WEBC_UTIL_LOG ("Failed to start response thread for client [%s:%u]\n",
                     remote_addr, remote_port);
         goto errorexit;
      }
      clientfd = -1;
      errcount = 0;
   }

   if (!g_exit_program && errcount) {
      WEBC_UTIL_LOG ("Aborting due to excessive error-count %" PRIu8 "\n", errcount);
      goto errorexit;
   }

   ret = EXIT_SUCCESS;

errorexit:

   free (logfile_name);

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

static const char *read_cline_opt (int argc, char **argv, const char *name)
{
   (void)argc;
   size_t namelen = strlen (name);

   for (size_t i=1; argv[i]; i++) {
      if ((memcmp (argv[i], "--", 2))!=0)
         continue;

      if ((strncmp (&argv[i][2], name, namelen))==0) {
         char *value = &argv[i][2+namelen];
         if (*value == '=')
            value++;
         argv[i][0] = 0;
         return value;
      }
   }
   return NULL;
}

/* ***************************************************************************
 * A daemon that will provide all the services that a client could ever want,
 * including:
 * 1. Client/user access control via group management.
 * 2. Calling of stored procedures on the database.
 * 3. File upload, download and modification.
 */

#define _POSIX_C_SOURCE 200809L
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

#include "web-add.h"
#include "util.h"
#include "config.h"
#include "resource.h"
#include "handler.h"

#include "ds_str.h"

static volatile sig_atomic_t g_exit_program = 0;
static size_t g_timeout = TIMEOUT_TO_SHUTDOWN;

static void print_help_msg (const char *option)
{
   static const struct {
      const char *option;
      const char *msg;
   } msgs[] = {
#define HELP(x,y)    { x, y }
   HELP ("help", "Display this screen"),
#undef HELP
   };
   for (size_t i=0; i<sizeof msgs/sizeof msgs[0]; i++) {
      if (option && (strcmp (option, msgs[i].option))==0) {
         printf ("%s\n", msgs[i].msg);
         return;
      }
      printf ("%s\n", msgs[i].msg);
   }
}

static void signal_handler (int n);
static int load_all_cline_opts (int argc, char **argv);

int main (int argc, char **argv)
{
   int ret = EXIT_FAILURE;

   char *logfile_name = NULL;

   uint32_t listen_port = 0;
   int backlog = 0;
   int listenfd = -1;
   uint8_t errcount = 0;

   int clientfd = -1;
   char *remote_addr = NULL;
   uint16_t remote_port = 0;

   /* *************************************************************
    *  Handle the command line arguments
    */
   load_all_cline_opts (argc, argv);
   if ((getenv ("webc_help"))!=NULL)
      print_help_msg (NULL);

   bool opt_unknown = false;
   for (size_t i=1; argv[i]; i++) {
      if (argv[i][0]) {
         UTIL_LOG ("Unknown option [%s]\n", argv[i]);
         opt_unknown = true;
      }
   }

   if (opt_unknown) {
      UTIL_LOG ("Aborting\n");
      return EXIT_FAILURE;
   }

   if (!(getenv ("webc_listen-port"))) {
      setenv ("webc_listen-port", DEFAULT_LISTEN_PORT, 1);
      UTIL_LOG ("No port number specified, using default [%s]\n", DEFAULT_LISTEN_PORT);
   }

   if (!(getenv ("webc_socket-backlog"))) {
      setenv ("webc_sock-backlog", DEFAULT_BACKLOG, 1);
      UTIL_LOG ("No backlog specified, using default [%s]\n", DEFAULT_BACKLOG);
   }

   if (!(getenv ("webc_logfile"))) {
      UTIL_LOG ("No logfile specified, logging to stderr\n");
   } else {
      int fd_logfile = -1;
      static const char *template = ".YYYYMMDDhhmmss";
      size_t logfile_namelen = 0;
      int fd_stderr = -1;

      time_t now = time (NULL);
      struct tm *time_fields = localtime (&now);
      if (!time_fields) {
         UTIL_LOG ("Failed to get local time: %m\n");
         goto errorexit;
      }

      logfile_namelen = strlen (getenv ("webc_logfile"))
                      + strlen (template)
                      + 1;
      if (!(logfile_name = malloc (logfile_namelen))) {
         UTIL_LOG ("OOM error constructing logfile name\n");
         goto errorexit;
      }
      snprintf (logfile_name, logfile_namelen, "%s."     // prefix
                                               "%04i"    // YYYY
                                               "%02i"    // MM
                                               "%02i"    // DD
                                               "%02i"    // hh
                                               "%02i"    // mm
                                               "%02i",   // ss
                                               getenv ("webc_logfile"),
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
         UTIL_LOG ("Cannot open logfile [%s]\n", logfile_name);
         goto errorexit;
      }
      if ((fd_stderr = fileno (stderr)) < 0) {
         UTIL_LOG ("Unable to get file descriptors for stderr\n");
         close (fd_logfile);
         goto errorexit;
      }
      if ((dup2 (fd_logfile, fd_stderr)) != fd_stderr) {
         UTIL_LOG ("Failed to dup() file descriptors for logging: %m\n");
         close (fd_logfile);
         goto errorexit;
      }
      fflush (stdout);
      fflush (stderr);
      UTIL_LOG ("Logging to [%s]\n", logfile_name);
      printf ("Logging to [%s]\n", logfile_name);
   }

   if ((chdir (DEFAULT_WEB_ROOT))!=0) {
      UTIL_LOG ("Failed to switch to web-root [%s]: %m\n", DEFAULT_WEB_ROOT);
      goto errorexit;
   }

   UTIL_LOG ("Starting the web.c server\n");

   /* ************************************************************** */

   if ((sscanf (getenv ("webc_listen-port"), "%u", &listen_port))!=1) {
      UTIL_LOG ("Listening port [%s] is invalid\n", getenv ("webc_listen-port"));
      goto errorexit;
   }
   if (listen_port >> 16 || listen_port==0) {
      UTIL_LOG ("Listening port [%u] is too large\n", listen_port);
      goto errorexit;
   }

   /* ************************************************************** */

   if ((signal (SIGINT, signal_handler))==SIG_ERR) {
      UTIL_LOG ("Failed to install signal handler: %m\n");
      goto errorexit;
   }

   if ((signal (SIGPIPE, SIG_IGN))==SIG_ERR) {
      UTIL_LOG ("Failed to block SIGPIPE: %m\n");
      goto errorexit;
   }

   /* ************************************************************** */

   if (!(web_add_init ())) {
      UTIL_LOG ("Failed to run the user-supplied initialisation\n");
      goto errorexit;
   }

   if (!(resource_global_handler_lock())) {
      UTIL_LOG ("Failed to aquire global resource handler lock\n");
      goto errorexit;
   }

   if (!(resource_global_handler_add ("handler_none",
                                      EXTENSION_NONE, pattern_SUFFIX,
                                      handler_none))) {
      UTIL_LOG ("Failed to add handler [%s]\n", EXTENSION_NONE);
      goto errorexit;
   }

   if (!(resource_global_handler_add ("handler_none",
                                      EXTENSION_DIR, pattern_SUFFIX,
                                      handler_dir))) {
      UTIL_LOG ("Failed to add handler [%s]\n", EXTENSION_DIR);
      goto errorexit;
   }

   if (!(resource_global_handler_add ("handler_none",
                                      EXTENSION_TEXT, pattern_SUFFIX,
                                      handler_static_file))) {
      UTIL_LOG ("Failed to add handler [%s]\n", EXTENSION_TEXT);
      goto errorexit;
   }

   if (!(resource_global_handler_add ("handler_none",
                                      EXTENSION_HTML, pattern_SUFFIX,
                                      handler_html))) {
      UTIL_LOG ("Failed to add handler [%s]\n", EXTENSION_HTML);
      goto errorexit;
   }

   if (!(web_add_load_handlers ())) {
      UTIL_LOG ("Failed to run the user-supplied load-handlers\n");
      goto errorexit;
   }

   if (!(resource_global_handler_unlock())) {
      UTIL_LOG ("Failed to release global resource handler lock\n");
      goto errorexit;
   }

   /* ************************************************************** */

   if ((listenfd = create_listener (listen_port, backlog)) < 0) {
      UTIL_LOG ("Unable to create a listener, aborting\n");
      goto errorexit;
   }

   UTIL_LOG ("Listening on %u q/%i\n", listen_port, backlog);

   errcount = 0;
   while (!g_exit_program && errcount < 5) {
      free (remote_addr);
      remote_addr = NULL;
      clientfd = accept_conn (listenfd, g_timeout,
                                        &remote_addr,
                                        &remote_port);
      if (clientfd == 0) { // Timeout
         errcount = 0;
         continue;
      }
      if (clientfd < 0) { // Error
         errcount++;
         UTIL_LOG ("Failed to accept(), errcount=%" PRIu8 "\n", errcount);
         continue;
      }

      if (!(handle_conn (clientfd, remote_addr, remote_port))) {
         UTIL_LOG ("Failed to start response thread for client [%s:%u]\n",
                     remote_addr, remote_port);
         goto errorexit;
      }
      clientfd = -1;
      errcount = 0;
   }

   if (!g_exit_program && errcount) {
      UTIL_LOG ("Aborting due to excessive error-count %" PRIu8 "\n", errcount);
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

static int load_all_cline_opts (int argc, char **argv)
{
   (void)argc;
   int retval = 0;

   for (size_t i=1; argv[i]; i++) {
      if ((memcmp (argv[i], "--", 2))!=0)
         continue;

      char *value = strchr (argv[i], '=');
      if (value && *value == '=')
         *value++  = 0;
      else
         value = "";

      // We do this so that we do not clash with a system-set environmetn variable.
      char *name = ds_str_cat ("webc_", argv[i], NULL);
      if (!name) {
         UTIL_LOG ("OOM Error\n");
         return false;
      }

      int rc = setenv (name, value, 1);
      free (name);

      if (!rc) {
         UTIL_LOG ("Failed to set option '%s'\n", argv[1]);
      }
      retval++;

      argv[i][0] = 0;
   }
   return retval;
}


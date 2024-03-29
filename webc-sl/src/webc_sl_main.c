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

#include "webc_web-add.h"
#include "webc_util.h"
#include "webc_config.h"
#include "webc_resource.h"
#include "webc_handler.h"

#include "webc_sl.h"

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

#define WEBC_HELP             ("help")
#define WEBC_LISTEN_PORT      ("listen-port")
#define WEBC_SOCKET_BACKLOG   ("socket-backlog")
#define WEBC_WEB_ROOT         ("web-root")
#define WEBC_LOGFILE          ("logfile")

static void signal_handler (int n);

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

   if ((webc_getenv (WEBC_HELP))!=NULL) {
      print_help_msg (NULL);
      return EXIT_SUCCESS;
   }

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

   if (!(webc_getenv (WEBC_LISTEN_PORT))) {
      webc_setenv (WEBC_LISTEN_PORT, DEFAULT_LISTEN_PORT);
      WEBC_UTIL_LOG ("No listen-port specified, using default [%s]\n", DEFAULT_LISTEN_PORT);
   }

   if (!(webc_getenv (WEBC_SOCKET_BACKLOG))) {
      webc_setenv (WEBC_SOCKET_BACKLOG, DEFAULT_BACKLOG);
      WEBC_UTIL_LOG ("No socket-backlog specified, using default [%s]\n", DEFAULT_BACKLOG);
   }

   if (!(webc_getenv (WEBC_WEB_ROOT))) {
      webc_setenv (WEBC_WEB_ROOT, DEFAULT_WEB_ROOT);
      WEBC_UTIL_LOG ("No web-root specified, using default [%s]\n", DEFAULT_WEB_ROOT);
   }

   if (!(webc_getenv (WEBC_LOGFILE))) {
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

      logfile_namelen = strlen (webc_getenv (WEBC_LOGFILE))
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
                                               webc_getenv (WEBC_LOGFILE),
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

   if ((chdir (webc_getenv (WEBC_WEB_ROOT)))!=0) {
      WEBC_UTIL_LOG ("Failed to switch to web-root [%s]: %m\n", webc_getenv (WEBC_WEB_ROOT));
      goto errorexit;
   }

   WEBC_UTIL_LOG ("Starting the web.c server\n");

   /* ************************************************************** */

   if ((sscanf (webc_getenv (WEBC_LISTEN_PORT), "%u", &listen_port))!=1) {
      WEBC_UTIL_LOG ("Listening port [%s] is invalid\n", webc_getenv (WEBC_LISTEN_PORT));
      goto errorexit;
   }
   if (listen_port >> 16 || listen_port==0) {
      WEBC_UTIL_LOG ("Listening port [%u] is too large\n", listen_port);
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

   /* *************************************************************** */
   if (!(webc_resource_global_handler_add ("auth", "/auth", pattern_PREFIX,
                                           webc_sl_handler_auth))) {
      WEBC_UTIL_LOG ("Failed to add handler [%s]\n", "/myapp");
      return false;
   }

   if (!(webc_resource_global_handler_add ("call_sp", "/call_sp", pattern_PREFIX,
                                           webc_sl_handler_call_sp))) {
      WEBC_UTIL_LOG ("Failed to add handler [%s]\n", "/myapp");
      return false;
   }

   if (!(webc_resource_global_handler_add ("call_native", "/call_native", pattern_PREFIX,
                                           webc_sl_handler_call_native))) {
      WEBC_UTIL_LOG ("Failed to add handler [%s]\n", "/myapp");
      return false;
   }

   if (!(webc_resource_global_handler_add ("call_fileAPI", "/call_fileAPI", pattern_PREFIX,
                                           webc_sl_handler_call_fileAPI))) {
      WEBC_UTIL_LOG ("Failed to add handler [%s]\n", "/myapp");
      return false;
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

   if ((listenfd = webc_create_listener (listen_port, backlog)) < 0) {
      WEBC_UTIL_LOG ("Unable to create a listener, aborting\n");
      goto errorexit;
   }

   WEBC_UTIL_LOG ("Listening on %u q/%i\n", listen_port, backlog);

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


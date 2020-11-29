#define _POSIX_C_SOURCE    200809L

#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

#include "webc_sl.h"

#include "webc_util.h"

#include "ds_str.h"

int load_all_cline_opts (int argc, char **argv)
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

      int rc = webc_setenv (argv[i], value);

      if (!rc) {
         WEBC_UTIL_LOG ("Failed to set option '%s'\n", argv[1]);
      }
      retval++;

      argv[i][0] = 0;
   }
   return retval;
}

const char *webc_getenv (const char *name)
{
   if (!name)
      return NULL;

   char *fullname = ds_str_cat ("webc_", name, NULL);
   if (!fullname) {
      WEBC_UTIL_LOG ("OOM error\n");
      return NULL;
   }

   char *ret = getenv (fullname);
   free (fullname);
   return ret;
}

int webc_setenv (const char *name, const char *value)
{
   if (!name)
      return -1;

   char *fullname = ds_str_cat ("webc_", name, NULL);
   if (!fullname) {
      WEBC_UTIL_LOG ("OOM error\n");
      return -2;
   }

   if ((setenv (fullname, value, 1))!=0) {
      WEBC_UTIL_LOG ("Failed to set environment variable [%s]: %m\n", fullname);
      return -3;
   }
   free (fullname);
   return 0;
}

int webc_sl_handler_auth (int                       fd,
                          char                     *remote_addr,
                          uint16_t                  remote_port,
                          enum webc_method_t        method,
                          enum webc_http_version_t  version,
                          const char               *resource,
                          char                    **rqst_headers,
                          webc_header_t            *rsp_headers,
                          char                     *vars)
{
   WEBC_UTIL_LOG ("auth call from %s:%u,method:%i,version:%i,resource:%s\n",
                     remote_addr, remote_port,
                     method, version, resource);
   static char buf[1024];
   ssize_t nbytes = 0;
   int flags = fcntl (fd, F_GETFL, 0);
   fcntl (fd, F_SETFL, flags | O_NONBLOCK);
   while ((nbytes = read (fd, buf, sizeof buf))>0) {
      printf ("%zu\n", nbytes);
      fwrite (buf, 1, nbytes, stdout);
   }
   fflush (stdout);
   return 501;
}

int webc_sl_handler_call_sp (int                       fd,
                             char                     *remote_addr,
                             uint16_t                  remote_port,
                             enum webc_method_t        method,
                             enum webc_http_version_t  version,
                             const char               *resource,
                             char                    **rqst_headers,
                             webc_header_t            *rsp_headers,
                             char                     *vars)
{
   WEBC_UTIL_LOG ("sp call from %s:%u,method:%i,version:%i,resource:%s\n",
                     remote_addr, remote_port,
                     method, version, resource);
   return 501;
}

int webc_sl_handler_call_native (int                       fd,
                                 char                     *remote_addr,
                                 uint16_t                  remote_port,
                                 enum webc_method_t        method,
                                 enum webc_http_version_t  version,
                                 const char               *resource,
                                 char                    **rqst_headers,
                                 webc_header_t            *rsp_headers,
                                 char                     *vars)
{
   WEBC_UTIL_LOG ("native call from %s:%u,method:%i,version:%i,resource:%s\n",
                     remote_addr, remote_port,
                     method, version, resource);
   return 501;
}

int webc_sl_handler_call_fileAPI (int                       fd,
                                  char                     *remote_addr,
                                  uint16_t                  remote_port,
                                  enum webc_method_t        method,
                                  enum webc_http_version_t  version,
                                  const char               *resource,
                                  char                    **rqst_headers,
                                  webc_header_t            *rsp_headers,
                                  char                     *vars)
{
   WEBC_UTIL_LOG ("fileAPI call from %s:%u,method:%i,version:%i,resource:%s\n",
                     remote_addr, remote_port,
                     method, version, resource);
   return 501;
}

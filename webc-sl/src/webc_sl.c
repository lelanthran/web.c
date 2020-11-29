#define _POSIX_C_SOURCE    200809L

#include <stdlib.h>
#include <string.h>

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
         UTIL_LOG ("Failed to set option '%s'\n", argv[1]);
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
      UTIL_LOG ("OOM error\n");
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
      UTIL_LOG ("OOM error\n");
      return -2;
   }

   if ((setenv (fullname, value, 1))!=0) {
      UTIL_LOG ("Failed to set environment variable [%s]: %m\n", fullname);
      return -3;
   }
   free (fullname);
   return 0;
}


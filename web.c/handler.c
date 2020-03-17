#include <stdio.h>

#include <unistd.h>

#include "handler.h"


bool handler_static_file (int                    fd,
                          char                  *remote_addr,
                          uint16_t               remote_port,
                          enum method_t          method,
                          enum http_version_t    version,
                          const char            *resource,
                          char                 **headers)
{
   bool error = true;
   FILE *inf = NULL;
   char buf[32];

   remote_addr = remote_addr;
   remote_port = remote_port;
   method = method;
   version = version;
   headers = headers;

   // TODO: Set the headers, response-type, etc.
   if (!(inf = fopen (resource, "r"))) {
      UTIL_LOG ("Failed to open [%s]: %m\n", resource);
      goto errorexit;
   }

   while (!feof (inf) && !ferror (inf)) {
      size_t nbytes = fread (buf, 1, sizeof buf, inf);
      if ((write (fd, buf, nbytes))==-1) {
         UTIL_LOG ("Failed to write [%s]: %m\n", resource);
         goto errorexit;
      }
   }

   error = false;

errorexit:
   if (inf)
      fclose (inf);

   return !error;
}


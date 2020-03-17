#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include "handler.h"


static int sendfile (int fd, const char *fname)
{
   int ret = 500;

   FILE *inf = NULL;
   char buf[32];

   if (!(inf = fopen (fname, "r"))) {
      UTIL_LOG ("Failed to open [%s]: %m\n", fname);
      if (errno == EACCES)
         ret = 403;
      goto errorexit;
   }

   while (!feof (inf) && !ferror (inf)) {
      size_t nbytes = fread (buf, 1, sizeof buf, inf);
      if ((write (fd, buf, nbytes))==-1) {
         UTIL_LOG ("Failed to write [%s]: %m\n", fname);
         goto errorexit;
      }
   }

   ret = 200;

errorexit:
   if (inf)
      fclose (inf);

   return ret;
}

int handler_static_file (int                    fd,
                          char                  *remote_addr,
                          uint16_t               remote_port,
                          enum method_t          method,
                          enum http_version_t    version,
                          const char            *resource,
                          char                 **headers)
{
   remote_addr = remote_addr;
   remote_port = remote_port;
   method = method;
   version = version;
   headers = headers;

   UTIL_LOG ("Sending static file\n");
   return sendfile (fd, resource);
}

int handler_html (int                    fd,
                   char                  *remote_addr,
                   uint16_t               remote_port,
                   enum method_t          method,
                   enum http_version_t    version,
                   const char            *resource,
                   char                 **headers)
{
   UTIL_LOG ("Sending html file\n");
   // TODO: SetCookie, SetHeader, return sendfile (fd, resource);
   return 200;
}


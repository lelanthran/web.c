#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include "handler.h"
#include "header.h"


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

   ret = 0;

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
   header_t *header = header_new ();
   if (!header)
      return 500;

   UTIL_LOG ("Sending html page\n");

   header_set (header, header_CONTENT_TYPE, "text/html");


   char *rsp = get_http_rspstr (200);

   write (fd, rsp, strlen (rsp));
   write (fd, "\r\n", 2);

   header_write (header, fd);

   int ret = sendfile (fd, resource);

   header_del (header);

   return ret;
}


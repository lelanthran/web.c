#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
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

   struct stat sb;

   header_t *header = header_new ();
   if (!header)
      return 500;

   if ((stat (resource, &sb))!=0) {
      THRD_LOG (remote_addr, remote_port, "Failed to stat [%s]\n", resource);
      return 500;
   }

   char slen[25];
   uint64_t st_size = sb.st_size;
   sprintf (slen, "%" PRIu64, st_size);

   header_set (header, header_CONTENT_TYPE, "application/octet-stream");
   header_set (header, header_CONTENT_LENGTH, slen);
   header_set (header, header_CONTENT_DISPOSITION, "attachment;");

   write (fd, get_http_rspstr (200), strlen (get_http_rspstr (200)));

   header_write (header, fd);
   header_del (header);

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

   const char *rsp = get_http_rspstr (200);
   write (fd, rsp, strlen (rsp));
   header_write (header, fd);
   header_del (header);

   int ret = sendfile (fd, resource);


   return ret;
}


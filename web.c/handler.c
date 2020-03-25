#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <sys/sendfile.h>

#include "handler.h"
#include "header.h"
#include "config.h"

static int get_filesize (const char *fname, char *dst_sizestr,
                                            uint64_t *dst_size64)
{
   struct stat sb;

   if ((stat (fname, &sb))!=0) {
      UTIL_LOG ("Failed to stat [%s]: %m\n", fname);
      if (errno == EACCES) {
         return 403;
      } else {
         return 404;
      }
   }

   uint64_t st_size = sb.st_size;

   if (dst_sizestr)
      sprintf (dst_sizestr, "%" PRIu64, st_size);

   if (dst_size64)
      *dst_size64 = st_size;

   return 200;
}

static int local_sendfile (int fd, const char *fname, uint64_t offset,
                                                      uint64_t count)
{
   int ret = 500;

   int in_fd = -1;

   if ((in_fd = open (fname, O_RDONLY, 0)) < 0) {
      UTIL_LOG ("Failed to open [%s]: %m\n", fname);
      if (errno == EACCES)
         ret = 403;
      goto errorexit;
   }

   off_t offs = (off_t)offset;
   size_t nbytes = count;
   ssize_t rc = 0;

   // TODO: This must be done in a loop.
   while ((rc = sendfile (fd, in_fd, &offs, nbytes)) != nbytes) {
      if (rc == -1) {
         UTIL_LOG ("Did not transmit all bytes\n");
         goto errorexit;
      }
      nbytes -= rc;
   }

   ret = 0;

errorexit:
   if (in_fd >= 0)
      close (in_fd);

   return ret;
}

int handler_static_file (int                    fd,
                         char                  *remote_addr,
                         uint16_t               remote_port,
                         enum method_t          method,
                         enum http_version_t    version,
                         const char            *resource,
                         char                 **rqst_headers,
                         header_t              *rsp_headers,
                         char                  *vars)
{
   int statcode = 0;
   remote_addr = remote_addr;
   remote_port = remote_port;
   method = method;
   version = version;
   rqst_headers = rqst_headers;

   header_t *header = header_new ();
   if (!header)
      return 500;


   char slen[25];
   uint64_t st_size;
   if ((statcode = get_filesize (resource, slen, &st_size))!=200) {
      THRD_LOG (remote_addr, remote_port, "Failed to access file [%s]\n",
                                          resource);
      header_del (header);
      return statcode;
   }

   header_set (header, header_CONTENT_TYPE, "application/octet-stream");
   header_set (header, header_CONTENT_LENGTH, slen);
   header_set (header, header_CONTENT_DISPOSITION, "attachment;");

   write (fd, get_http_rspstr (200), strlen (get_http_rspstr (200)));

   header_write (header, fd);
   header_del (header);

   UTIL_LOG ("Sending static file\n");

   return local_sendfile (fd, resource, 0, st_size);
}

int handler_html (int                    fd,
                  char                  *remote_addr,
                  uint16_t               remote_port,
                  enum method_t          method,
                  enum http_version_t    version,
                  const char            *resource,
                  char                 **rqst_headers,
                  header_t              *rsp_headers,
                  char                  *vars)
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

   int ret = local_sendfile (fd, resource, 0, (size_t)-1);


   return ret;
}

int handler_none (int                    fd,
                  char                  *remote_addr,
                  uint16_t               remote_port,
                  enum method_t          method,
                  enum http_version_t    version,
                  const char            *resource,
                  char                 **rqst_headers,
                  header_t              *rsp_headers,
                  char                  *vars)
{
   struct stat sb;
   int (*statfunc) (const char *pathname, struct stat *statbuf);

   statfunc = FOLLOW_SYMLINKS ? lstat : stat;

   if ((statfunc (resource, &sb))!=0) {
      THRD_LOG (remote_addr, remote_port, "Failed to stat [%s]\n", resource);
      return 404;
   }

   if (S_ISREG (sb.st_mode)) {
      return handler_static_file (fd, remote_addr, remote_port, method,
                                  version, resource,
                                  rqst_headers, rsp_headers,
                                  vars);
   }

   if (S_ISDIR (sb.st_mode)) {
      return handler_dir (fd, remote_addr, remote_port, method,
                          version, resource,
                          rqst_headers,
                          rsp_headers,
                          vars);
   }

   return 500;
}

int handler_dir (int                    fd,
                 char                  *remote_addr,
                 uint16_t               remote_port,
                 enum method_t          method,
                 enum http_version_t    version,
                 const char            *resource,
                 char                 **rqst_headers,
                 header_t              *rsp_headers,
                 char                  *vars)
{

   struct stat sb;
   char *index_html = NULL;
   size_t index_html_len = 0;

   if ((stat (resource, &sb))!=0) {
      THRD_LOG (remote_addr, remote_port, "Failed to stat [%s]\n", resource);
      return 404;
   }

   if (!(S_ISDIR (sb.st_mode))) {
      THRD_LOG (remote_addr, remote_port, "Not a directory [%s]\n", resource);
      return 500;
   }

   index_html_len = strlen (resource) + 1 + strlen (DEFAULT_INDEX_FILE) + 1;
   if (!(index_html = malloc (index_html_len))) {
      THRD_LOG (remote_addr, remote_port, "Out of memory [%s]\n", resource);
      return 500;
   }

   strcpy (index_html, resource);
   strcat (index_html, DEFAULT_INDEX_FILE);

   THRD_LOG (remote_addr, remote_port, "Trying [%s]\n", index_html);

   if ((stat (index_html, &sb))!=0) {
      int ret = handler_dirlist (fd, remote_addr, remote_port, method, version,
                                 resource,
                                 rqst_headers,
                                 rsp_headers,
                                 vars);
      free (index_html);
      return ret;
   }

   int ret =  handler_html (fd, remote_addr, remote_port, method, version,
                            index_html,
                            rqst_headers,
                            rsp_headers,
                            vars);

   free (index_html);

   return ret;
}

int handler_dirlist (int                    fd,
                     char                  *remote_addr,
                     uint16_t               remote_port,
                     enum method_t          method,
                     enum http_version_t    version,
                     const char            *resource,
                     char                 **rqst_headers,
                     header_t              *rsp_headers,
                     char                  *vars)
{
   static const char *header =
      "<html>"
      "  <body>"
      "  <table>"
      "     <tr>"
      "        <th></th>"
      "        <th>Name</th>"
      "        <th>Last Modified</th>"
      "        <th>Size</th>"
      "        <th>Description</th>"
      "     </tr>";

   static const char *row =
      "<tr>"
      "  <td></td>"
      "  <td></td>"
      "  <td></td>"
      "  <td></td>"
      "  <td></td>"
      "</tr>";

   static const char *footer =
      "  </table>"
      "  <p>Powered by <em>" APPLICATION_ID "/" VERSION_STRING "</em>"
      "  </body>"
      "</html>";

   struct stat sb;
   DIR *dirp = NULL;
   struct dirent *de = NULL;

   if (!(dirp = opendir (resource))) {
      THRD_LOG (remote_addr, remote_port, "Unable to opendir [%s]\n", resource);
      if (errno==EACCES)
         return 403;
      return 500;
   }

   const char *rsp = get_http_rspstr (200);
   write (fd, rsp, strlen (rsp));

   write (fd, "Content-type: text/html\r\n\r\n", 27);

   write (fd, "<html>", 6);
   write (fd, "<body>", 6);
   write (fd, "<ul>", 4);
   while ((de = readdir (dirp))!=NULL) {
      write (fd, "<li>", 4);
      write (fd, de->d_name, strlen (de->d_name));
      write (fd, "</li>", 5);
   }
   write (fd, "</ul>", 5);
   write (fd, "</body>", 7);
   write (fd, "</html>", 7);

   // header_write (header, fd);

   closedir (dirp);

   return 0;
}



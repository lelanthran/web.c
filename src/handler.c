#define _POSIX_C_SOURCE 200809L

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
   off_t offs = (off_t)offset;
   size_t nbytes = count;
   ssize_t rc = 0;

   if ((in_fd = open (fname, O_RDONLY, 0)) < 0) {
      UTIL_LOG ("Failed to open [%s]: %m\n", fname);
      if (errno == EACCES)
         ret = 403;
      goto errorexit;
   }

   // TODO: This must be done in a loop.
   while ((rc = sendfile (fd, in_fd, &offs, nbytes)) != (ssize_t)nbytes) {
      if (rc == -1) {
         UTIL_LOG ("Did not transmit all bytes\n");
         goto errorexit;
      }
      nbytes -= rc;
   }

   ret = 200;

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
   (void) vars;

   int statcode = 0;
   remote_addr = remote_addr;
   remote_port = remote_port;
   method = method;
   version = version;
   rqst_headers = rqst_headers;

   char slen[25];
   uint64_t st_size;
   if ((statcode = get_filesize (resource, slen, &st_size))!=200) {
      THRD_LOG (remote_addr, remote_port, "Failed to access file [%s]\n",
                                          resource);
      return statcode;
   }

   header_set (rsp_headers, header_CONTENT_TYPE, "application/octet-stream");
   header_set (rsp_headers, header_CONTENT_LENGTH, slen);
   header_set (rsp_headers, header_CONTENT_DISPOSITION, "attachment;");

   write (fd, get_http_rspstr (200), strlen (get_http_rspstr (200)));

   header_write (rsp_headers, fd);

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
   (void) method;
   (void) version;
   (void) resource;
   (void) rqst_headers;
   (void) rsp_headers;
   (void) vars;

   UTIL_LOG ("Sending html page\n");

   char slen[25];
   uint64_t st_size;
   int statcode = 0;
   if ((statcode = get_filesize (resource, slen, &st_size))!=200) {
      THRD_LOG (remote_addr, remote_port, "Failed to access file [%s]\n",
                                          resource);
      return statcode;
   }

   header_set (rsp_headers, header_CONTENT_TYPE, "text/html");
   header_set (rsp_headers, header_CONTENT_LENGTH, slen);

   const char *rsp = get_http_rspstr (200);
   write (fd, rsp, strlen (rsp));
   header_write (rsp_headers, fd);

   return local_sendfile (fd, resource, 0, st_size);
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

   if (resource[0] == 0) {
      resource = ".";
   }

   if ((stat (resource, &sb))!=0) {
      THRD_LOG (remote_addr, remote_port,
                  "In dir [%s] Failed to stat [%s]: %m\n", getenv ("PWD"), resource);
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

char **get_dirlist (const char *addr, uint16_t port, const char *path)
{
   DIR *dirp = NULL;
   struct dirent *de = NULL;
   size_t nrecs = 0;

   if (!(dirp = opendir (path))) {
      THRD_LOG (addr, port, "Unable to opendir [%s]\n", path);
      return NULL;
   }

   while ((de = readdir (dirp)))
      nrecs++;

   rewinddir (dirp);

   char **ret = calloc (nrecs + 1, sizeof *ret);
   if (!ret) {
      closedir (dirp);
      return NULL;
   }

   size_t idx = 0;
   while ((de = readdir (dirp))) {
      if ((memcmp (de->d_name, ".", 2))==0)
         continue;

      if (!(ret[idx] = malloc (strlen (de->d_name) + 2))) {
         for (size_t i=0; ret[i]; i++)
            free (ret[i]);
         free (ret);
         closedir (dirp);
         return NULL;
      }

      struct stat sb;
      char *tmp = malloc (strlen (path) + '/' + strlen (de->d_name) + 1);
      if (!tmp) {
         free (ret);
         closedir (dirp);
         return NULL;
      }
      strcpy (tmp, path);
      strcat (tmp, "/");
      strcat (tmp, de->d_name);
      memset (&sb, 0, sizeof sb);
      stat (tmp, &sb);
      free (tmp);

      strcpy (ret[idx], de->d_name);
      if (S_ISDIR (sb.st_mode)) {
         strcat (ret[idx], "/");
      }
      idx++;
   }

   closedir (dirp);
   return ret;
}

static int cb_strsort (const void *lhs, const void *rhs)
{
   const char * const *slhs = lhs,
              * const *srhs = rhs;
   return strcmp (*slhs, *srhs);
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
   (void) method;
   (void) version;
   (void) rqst_headers;
   (void) rsp_headers;
   (void) vars;

   static const char *header =
      "<html>"
      "  <body>"
      " <p style='font-size:large'>Directory index for <b><em>%s</b></em></p>"
      "  <hr>"
      "  <table>";

   static const char *footer =
      "  </table>"
      "  <hr>"
      "  <p><em>Powered by " APPLICATION_ID "/" VERSION_STRING "</em>"
      "  </body>"
      "</html>";

   char *row = NULL;
   size_t rowlen = 0;

   char **dirlist = get_dirlist (remote_addr, remote_port, resource);
   char *tmp_resource = strdup (resource);

   const char *rsp = get_http_rspstr (200);
   write (fd, rsp, strlen (rsp));

   write (fd, "Content-type: text/html\r\n\r\n", 27);

   if (!dirlist || !tmp_resource) {
      dprintf (fd, "Unrecoverable error\n");
      return 500;
   }

   size_t nrecs = 0;
   for (size_t i=0; dirlist[i]; i++)
      nrecs++;

   qsort (dirlist, nrecs, sizeof dirlist[0], cb_strsort);

   dprintf (fd, header, resource);

   size_t reslen = strlen (tmp_resource);

   if (tmp_resource[reslen-1] == '/')
      tmp_resource[reslen-1] = 0;

   for (size_t i=0; dirlist[i]; i++) {
      bool rc;
      if ((memcmp (dirlist[i], "..", 3))==0) {
         char *parent = strdup (tmp_resource);
         if (!parent) {
            THRD_LOG (remote_addr, remote_port, "OOM error [%s]\n", dirlist[i]);
            free (tmp_resource);
            return 500;
         }
         char *term = strrchr (parent, '/');
         if (term)
            *term = 0;
         else
            *parent = 0;

         if (!*parent) {
            parent[0] = '/';
            parent[1] = 0;
         }

         const char *fmts = "<tr><td><a href='%s'>%s</a></td></tr>\n";
         if (!(strchr (parent, '/')))
            fmts = "<tr><td><a href='/%s/'>%s</a></td></tr>\n";

         rc = util_sprintf (&row, &rowlen, fmts,
                                 parent,
                                 dirlist[i]);
         free (parent);
      } else {
         rc = util_sprintf (&row, &rowlen, "<tr><td>"
                                 "<a href='/%s/%s'>%s</a>"
                                 "</td></tr>\n",
                                 tmp_resource,
                                 dirlist[i],
                                 dirlist[i]);
      }

      dprintf (fd, "%s", row); // Will get printed twice in case of error.
      free (row);
      row = NULL;
      if (!rc) {
         THRD_LOG (remote_addr, remote_port, "OOM error [%s]\n", dirlist[i]);
         free (tmp_resource);
         return 500;
      }
   }
   dprintf (fd, "%s\n", footer);

   free (tmp_resource);

   for (size_t i=0; dirlist[i]; i++) {
      free (dirlist[i]);
   }
   free (dirlist);

   return 200;
}


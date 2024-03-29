#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include <pthread.h>

#include "webc_resource.h"
#include "webc_util.h"
#include "webc_config.h"
#include "webc_header.h"

static enum webc_method_t get_rqst_method (const char *rqst_line)
{
   static const struct {
      const char *name;
      enum webc_method_t method;
   } methods[] = {
      { "UNKNOWN",   webc_method_UNKNOWN  },
      { "GET",       webc_method_GET      },
      { "HEAD",      webc_method_HEAD     },
      { "POST",      webc_method_POST     },
      { "PUT",       webc_method_PUT      },
      { "DELETE",    webc_method_DELETE   },
      { "TRACE",     webc_method_TRACE    },
      { "OPTIONS",   webc_method_OPTIONS  },
      { "CONNECT",   webc_method_CONNECT  },
      { "PATCH",     webc_method_PATCH    },
   };

   for (size_t i=0; i<sizeof methods/sizeof methods[0]; i++) {
      if ((memcmp (methods[i].name, rqst_line, strlen (methods[i].name)))==0) {
         return methods[i].method;
      }
   }

   return webc_method_UNKNOWN;
}

static enum webc_http_version_t get_rqst_version (const char *rqst_line)
{
   static const struct {
      const char              *name;
      enum webc_http_version_t version;
   } versions[] = {
      { "UNKNOWN",      webc_http_version_UNKNOWN  },
      { "XXXHTTP/0.9",  webc_http_version_0_9      },
      { "HTTP/1.0",     webc_http_version_1_0      },
      { "HTTP/1.1",     webc_http_version_1_1      },
      { "XXXHTTP/2",    webc_http_version_2_0      },
      { "XXXHTTP/3",    webc_http_version_3_0      },
   };

   char *tmp = strstr (rqst_line, "HTTP/");
   if (!tmp)
      return webc_http_version_UNKNOWN;

   for (size_t i=0; i<sizeof versions/sizeof versions[0]; i++) {
      if ((memcmp (versions[i].name, tmp, strlen (versions[i].name)))==0) {
         return versions[i].version;
      }
   }

   return webc_http_version_UNKNOWN;
}

static char *get_rqst_resource (const char *rqst_line)
{
   char *start = strchr (rqst_line, ' ');
   if (!start)
      return NULL;
   start++;
   char *end = strchr (start, '?');
   if (!end)
      end = strchr (start, ' ');
   if (!end)
      return NULL;

   ptrdiff_t len = end - start;

   char *ret = malloc (len + 2);
   if (!ret)
      return NULL;

   strncpy (ret, start, len);
   ret[len] = 0;

   char *dblslash;
   while ((dblslash = strstr (ret, "//"))) {
      memmove (dblslash, &dblslash[1], (strlen (&dblslash[1]) + 1));
   }

   return ret;
}

static char *parse_urlencoded_string (const char *string, size_t len)
{
#warning TODO: Implement encoder/decoder for url strings
   (void)string;
   (void)len;
   return NULL;
}

static char *get_rqst_getvars (const char *rqst_line)
{
   char *start = strchr (rqst_line, '?');
   if (!start)
      return NULL;
   start++;
   char *end = strchr (start, ' ');
   if (!end)
      return NULL;

   ptrdiff_t len = end - start;

   char *ret = malloc (len + 2);
   if (!ret)
      return NULL;

   strncpy (ret, start, len);
   ret[len] = 0;
   return ret;
}

/* ******************************************************************* */


/* Should really define __GNU_SOURCE or set the correct -std flag instead of
 * doing this.
 */
int accept4(int sockfd, struct sockaddr *addr,
            socklen_t *addrlen, int flags);

int webc_create_listener (uint32_t portnum, int backlog)
{
   struct sockaddr_in addr;

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = INADDR_ANY;
   addr.sin_port = htons (portnum & 0xffff);
   int fd = -1;

   fd = socket (AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
   if (fd < 0) {
      WEBC_UTIL_LOG ("socket() failed: %m\n");
      return -1;
   }
   int enable = 1;
   if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
      WEBC_UTIL_LOG ("setsockopt(SO_REUSEADDR) failed, continuing");

   if (bind (fd, (struct sockaddr *)&addr, sizeof addr)!=0) {
      WEBC_UTIL_LOG ("bind() failed: %m\n");
      close (fd);
      return -1;
   }
   if (listen (fd, backlog)!=0) {
      WEBC_UTIL_LOG ("listen() failed: %m\n");
      close (fd);
      return -1;
   }
   return fd;
}


int webc_accept_conn (int listenfd, size_t timeout,
                               char **remote_addr,
                               uint16_t *remote_port)
{
   struct sockaddr_in ret;
   socklen_t retlen = sizeof ret;
   int retval = -1;

   memset (&ret, 0xff, sizeof ret);

   fd_set fds[3]; // Read/write/except
   struct timeval tv = { (long int)timeout , 0 };
   for (size_t i=0; i<sizeof fds/sizeof fds[0]; i++) {
      FD_ZERO (&fds[i]);
      FD_SET (listenfd, &fds[i]);
   }
   int r = select (listenfd + 1, &fds[0], &fds[1], &fds[2], &tv);
   if (r == 0) {
      return 0;
   }
   if (r < 0) {
      return -1;
   }

   retval = accept4 (listenfd, (struct sockaddr *)&ret, &retlen, SOCK_CLOEXEC);
   if (retval <= 0) {
      return -1;
   }

   if (remote_addr) {
      *remote_addr = malloc (16);
      if (!*remote_addr) {
         return retval;
      }

      uint8_t bytes[4];
      bytes[3] = (ret.sin_addr.s_addr >> 24) & 0xff;
      bytes[2] = (ret.sin_addr.s_addr >> 16) & 0xff;
      bytes[1] = (ret.sin_addr.s_addr >>  8) & 0xff;
      bytes[0] = (ret.sin_addr.s_addr      ) & 0xff;
      sprintf (*remote_addr, "%u.%u.%u.%u", bytes[0],
                                            bytes[1],
                                            bytes[2],
                                            bytes[3]);
   }

   if (remote_port) {
      *remote_port = ntohs (ret.sin_port);
   }
   return retval;
}


struct thread_args_t {
   int fd;
   char *remote_addr;
   uint16_t remote_port;
};

static void thread_args_del (struct thread_args_t *args)
{
   if (args) {
      free (args->remote_addr);
      free (args);
   }
}

static struct thread_args_t *thread_args_new (int fd, const char *remote_addr,
                                                      int16_t remote_port)
{
   struct thread_args_t *ret = NULL;

   if (!(ret = calloc (1, sizeof *ret))) {
      return NULL;
   }

   ret->fd = fd;
   ret->remote_addr = strdup (remote_addr);
   ret->remote_port = remote_port;

   if (!ret->remote_addr) {
      thread_args_del (ret);
      ret = NULL;
   }

   return ret;
}

/* ****************************************************************** */

static bool fd_read_line (int fd, char **dst, size_t *dstlen)
{
   char *line = NULL;
   size_t line_len = 0;
   char c;

   free (*dst);
   *dst = NULL;
   *dstlen = 0;

   while ((read (fd, &c, 1))==1) {
      char *tmp = realloc (line, line_len + 2);
      if (!tmp) {
         WEBC_UTIL_LOG ("OOM error\n");
         free (line);
         return false;
      }
      line = tmp;
      line[line_len++] = c;
      if (c == '\n' && line[line_len-2] == '\r') {
         line_len -= 2;
         line[line_len] = 0;
         break;
      }
   }

   *dst = line;
   *dstlen = line_len;
   return true;
}

const char *webc_get_http_rspstr (int status)
{
   static const struct {
      int status;
      const char *string;
   } statuses[] = {
      {  100,  "HTTP/1.1 100 Continue\r\n"                              },
      {  101,  "HTTP/1.1 101 Switching Protocols\r\n"                   },
      {  102,  "HTTP/1.1 102 Processing\r\n"                            },
      {  103,  "HTTP/1.1 103 Checkpoint\r\n"                            },
      {  103,  "HTTP/1.1 103 Early Hints\r\n"                           },
      {  200,  "HTTP/1.1 200 OK\r\n"                                    },
      {  201,  "HTTP/1.1 201 Created\r\n"                               },
      {  202,  "HTTP/1.1 202 Accepted\r\n"                              },
      {  203,  "HTTP/1.1 203 Non-Authoritative Information\r\n"         },
      {  204,  "HTTP/1.1 204 No Content\r\n"                            },
      {  205,  "HTTP/1.1 205 Reset Content\r\n"                         },
      {  206,  "HTTP/1.1 206 Partial Content\r\n"                       },
      {  207,  "HTTP/1.1 207 Multi-Status\r\n"                          },
      {  208,  "HTTP/1.1 208 Already Reported\r\n"                      },
      {  218,  "HTTP/1.1 218 This is fine\r\n"                          },
      {  226,  "HTTP/1.1 226 IM Used\r\n"                               },
      {  300,  "HTTP/1.1 300 Multiple Choices\r\n"                      },
      {  301,  "HTTP/1.1 301 Moved Permanently\r\n"                     },
      {  302,  "HTTP/1.1 302 Found\r\n"                                 },
      {  303,  "HTTP/1.1 303 See Other\r\n"                             },
      {  304,  "HTTP/1.1 304 Not Modified\r\n"                          },
      {  305,  "HTTP/1.1 305 Use Proxy\r\n"                             },
      {  306,  "HTTP/1.1 306 Switch Proxy\r\n"                          },
      {  307,  "HTTP/1.1 307 Temporary Redirect\r\n"                    },
      {  308,  "HTTP/1.1 308 Permanent Redirect\r\n"                    },
      {  400,  "HTTP/1.1 400 Bad Request\r\n"                           },
      {  401,  "HTTP/1.1 401 Unauthorized\r\n"                          },
      {  402,  "HTTP/1.1 402 Payment Required\r\n"                      },
      {  403,  "HTTP/1.1 403 Forbidden\r\n"                             },
      {  404,  "HTTP/1.1 404 Not Found\r\n"                             },
      {  405,  "HTTP/1.1 405 Method Not Allowed\r\n"                    },
      {  406,  "HTTP/1.1 406 Not Acceptable\r\n"                        },
      {  407,  "HTTP/1.1 407 Proxy Authentication Required \r\n"        },
      {  408,  "HTTP/1.1 408 Request Timeout\r\n"                       },
      {  409,  "HTTP/1.1 409 Conflict\r\n"                              },
      {  410,  "HTTP/1.1 410 Gone\r\n"                                  },
      {  411,  "HTTP/1.1 411 Length Required\r\n"                       },
      {  412,  "HTTP/1.1 412 Precondition Failed \r\n"                  },
      {  413,  "HTTP/1.1 413 Payload Too Large \r\n"                    },
      {  414,  "HTTP/1.1 414 URI Too Long \r\n"                         },
      {  415,  "HTTP/1.1 415 Unsupported Media Type \r\n"               },
      {  416,  "HTTP/1.1 416 Range Not Satisfiable \r\n"                },
      {  417,  "HTTP/1.1 417 Expectation Failed\r\n"                    },
      {  418,  "HTTP/1.1 418 I'm a teapot \r\n"                         },
      {  419,  "HTTP/1.1 419 Page Expired \r\n"                         },
      {  420,  "HTTP/1.1 420 Enhance Your Calm \r\n"                    },
      {  420,  "HTTP/1.1 420 Method Failure \r\n"                       },
      {  421,  "HTTP/1.1 421 Misdirected Request \r\n"                  },
      {  422,  "HTTP/1.1 422 Unprocessable Entity \r\n"                 },
      {  423,  "HTTP/1.1 423 Locked \r\n"                               },
      {  424,  "HTTP/1.1 424 Failed Dependency \r\n"                    },
      {  425,  "HTTP/1.1 425 Too Early \r\n"                            },
      {  426,  "HTTP/1.1 426 Upgrade Required\r\n"                      },
      {  428,  "HTTP/1.1 428 Precondition Required \r\n"                },
      {  429,  "HTTP/1.1 429 Too Many Requests \r\n"                    },
      {  430,  "HTTP/1.1 430 Request Header Fields Too Large \r\n"      },
      {  431,  "HTTP/1.1 431 Request Header Fields Too Large \r\n"      },
      {  440,  "HTTP/1.1 440 Login Time-out\r\n"                        },
      {  444,  "HTTP/1.1 444 No Response\r\n"                           },
      {  449,  "HTTP/1.1 449 Retry With\r\n"                            },
      {  450,  "HTTP/1.1 450 Blocked by Windows Parental Controls \r\n" },
      {  451,  "HTTP/1.1 451 Redirect\r\n"                              },
      {  451,  "HTTP/1.1 451 Unavailable For Legal Reasons \r\n"        },
      {  494,  "HTTP/1.1 494 Request header too large\r\n"              },
      {  495,  "HTTP/1.1 495 SSL Certificate Error\r\n"                 },
      {  496,  "HTTP/1.1 496 SSL Certificate Required\r\n"              },
      {  497,  "HTTP/1.1 497 HTTP Request Sent to HTTPS Port\r\n"       },
      {  498,  "HTTP/1.1 498 Invalid Token \r\n"                        },
      {  499,  "HTTP/1.1 499 Client Closed Request\r\n"                 },
      // {  499,  "HTTP/1.1 499 Token Required \r\n"                       },
      {  500,  "HTTP/1.1 500 Internal Server Error\r\n"                 },
      {  501,  "HTTP/1.1 501 Not Implemented\r\n"                       },
      {  502,  "HTTP/1.1 502 Bad Gateway\r\n"                           },
      {  503,  "HTTP/1.1 503 Service Unavailable\r\n"                   },
      {  504,  "HTTP/1.1 504 Gateway Timeout\r\n"                       },
      {  505,  "HTTP/1.1 505 HTTP Version Not Supported\r\n"            },
      {  506,  "HTTP/1.1 506 Variant Also Negotiates \r\n"              },
      {  507,  "HTTP/1.1 507 Insufficient Storage \r\n"                 },
      {  508,  "HTTP/1.1 508 Loop Detected \r\n"                        },
      {  509,  "HTTP/1.1 509 Bandwidth Limit Exceeded \r\n"             },
      {  510,  "HTTP/1.1 510 Not Extended \r\n"                         },
      {  511,  "HTTP/1.1 511 Network Authentication Required \r\n"      },
      {  520,  "HTTP/1.1 520 Web Server Returned an Unknown Error\r\n"  },
      {  521,  "HTTP/1.1 521 Web Server Is Down\r\n"                    },
      {  522,  "HTTP/1.1 522 Connection Timed Out\r\n"                  },
      {  523,  "HTTP/1.1 523 Origin Is Unreachable\r\n"                 },
      {  524,  "HTTP/1.1 524 A Timeout Occurred\r\n"                    },
      {  525,  "HTTP/1.1 525 SSL Handshake Failed\r\n"                  },
      {  526,  "HTTP/1.1 526 Invalid SSL Certificate\r\n"               },
      {  527,  "HTTP/1.1 527 Railgun Error\r\n"                         },
      {  529,  "HTTP/1.1 529 Site is overloaded\r\n"                    },
      {  530,  "HTTP/1.1 530 Site is frozen\r\n"                        },
      {  598,  "HTTP/1.1 598  Network read timeout error\r\n"           },
   };

   for (size_t i=0; i<sizeof statuses/sizeof statuses[0]; i++) {
      if (statuses[i].status == status)
         return statuses[i].string;
   }

   return "HTTP/1.1 500 Internal Server Error\r\n";
}

static void *thread_func (void *ta)
{
   int status = 500;
   const char *rsp_line = NULL;

   struct thread_args_t *args = ta;

   enum webc_method_t method = 0;
   char *org_resource = NULL;
   char *resource = NULL;
   char *getvars = NULL;
   const char *content_type = NULL;
   enum webc_http_version_t version = 0;
   webc_resource_handler_t *webc_resource_handler = NULL;

   char *rqst_line = NULL;
   size_t rqst_line_len = 0;

   char *rqst_headers[MAX_HTTP_HEADERS];
   size_t rqst_header_lens[MAX_HTTP_HEADERS];

   webc_header_t *rsp_headers = NULL;

   size_t i;

   memset (rqst_headers, 0, MAX_HTTP_HEADERS * sizeof rqst_headers[0]);
   memset (rqst_header_lens, 0, MAX_HTTP_HEADERS * sizeof rqst_header_lens[0]);

   if (!(rsp_headers = webc_header_new ())) {
      WEBC_THRD_LOG (args->remote_addr, args->remote_port,
                  "Failed to create header object\n");
      goto errorexit;
   }

   if (!(fd_read_line (args->fd, &rqst_line, &rqst_line_len)) ||
       !rqst_line ||
       !rqst_line_len) {
      WEBC_THRD_LOG (args->remote_addr, args->remote_port,
                "Malformed request line: [%s]. Aborting.\n", rqst_line);
      status = 400;
      goto errorexit;
   }

   WEBC_TS_LOG ("[%s:%u] [%s]\n", args->remote_addr, args->remote_port, rqst_line);

   for (i=0; i<MAX_HTTP_HEADERS; i++) {
      if (!(fd_read_line (args->fd, &rqst_headers[i], &rqst_header_lens[i]))) {
         WEBC_THRD_LOG (args->remote_addr, args->remote_port,
                   "Unexpected end of rqst_headers");
         status = 400;
         goto errorexit;
      }
      if (rqst_header_lens[i]==0) {
         // Reached the empty line, everything after this is the message body
         break;
      }
   }

   if (i >= MAX_HTTP_HEADERS) {
      WEBC_THRD_LOG (args->remote_addr, args->remote_port,
                "Too many rqst_headers sent (%zu), ignoring the rest\n", i);
   }

#if 0
   WEBC_THRD_LOG (args->remote_addr, args->remote_port, "Collected all rqst_headers\n");
   WEBC_THRD_LOG (args->remote_addr, args->remote_port,
             "rqst: [%s]\n", rqst_line);

   for (size_t i=0; rqst_headers[i]; i++) {
      WEBC_THRD_LOG (args->remote_addr, args->remote_port,
                "header: [%s]\n", rqst_headers[i]);
   }
#endif

   method = get_rqst_method (rqst_line);
   org_resource = get_rqst_resource (rqst_line);
   version = get_rqst_version (rqst_line);
   getvars = get_rqst_getvars (rqst_line);
   webc_resource_handler = webc_resource_handler_find (org_resource);

   WEBC_THRD_LOG (args->remote_addr, args->remote_port,
                  "method        [%i]\n"
                  "org_resource  [%s]\n"
                  "version       [%i]\n"
                  "getvars       [%s]\n",
                     method, org_resource, version, getvars);

   if (!method || !org_resource || !version || !webc_resource_handler) {
      WEBC_THRD_LOG (args->remote_addr, args->remote_port,
                "Unrecognised method, version or resource [%s]\n",
                 rqst_line);
      status = 400;
      goto errorexit;
   }

   if ((strstr (org_resource, ".."))!=NULL) {
      WEBC_THRD_LOG (args->remote_addr, args->remote_port,
                "Attempt to access parent directory [%s]\n",
                 rqst_line);
      status = 403;
      goto errorexit;
   }

   resource = (org_resource[0]=='/') ? &org_resource[1] : org_resource;

   /* Check if request is a POSTed form data (either multipart/form-data or
    * application/x-www-form-urlencoded), and if it is we grab the individual
    * parts of the body as form data or we parse the body as urlencoded,
    * respectively).
    */
   content_type = headerlist_find (rqst_headers,
                                               webc_header_CONTENT_TYPE);
   if (content_type && method == webc_method_POST) {
      // Must see if other methods can send forms
      static const char *mform_data = "multipart/form-data",
                        *awww_form = "application/x-www-form-urlencoded";

      if ((strnicmp (content_type, mform_data, strlen (mform_data)))==0) {
         // TODO: read the multipart form data
      }

      if ((strnicmp (content_type, awww_form, strlen (awww_form)))==0) {
         // TODO: read the POSTed form data
      }
   }
   status = webc_resource_handler (args->fd, args->remote_addr, args->remote_port,
                                   method, version, resource,
                                   rqst_headers, rsp_headers,
                                   getvars);

   WEBC_TS_LOG ("[%s:%u] =>[%i]\n", args->remote_addr, args->remote_port, status);

errorexit:

   if (!status || status!=200) {
      rsp_line = webc_get_http_rspstr (status);
      write (args->fd, rsp_line, strlen (rsp_line));
      write (args->fd, "\r\n\r\n", 4);
      char outbuf[100];
      snprintf (outbuf, sizeof outbuf, "Error: %i\n", status);
      write (args->fd, outbuf, strlen (outbuf));
      write (args->fd, "\r\n\r\n", 4);
   }

   free (rqst_line);
   free (org_resource);
   free (getvars);

   for (i=0; i<MAX_HTTP_HEADERS; i++) {
      free (rqst_headers[i]);
   }
   webc_header_del (rsp_headers);

   shutdown (args->fd, SHUT_RDWR);
   close (args->fd);

   WEBC_THRD_LOG (args->remote_addr, args->remote_port, "Ending thread\n");
   thread_args_del (args);

   return NULL;
}

bool webc_handle_conn (int fd, char *remote_addr, uint16_t remote_port)
{
   bool error = true;

   pthread_attr_t attr;
   pthread_t thread;
   struct thread_args_t *args = NULL;

   if (!(args = thread_args_new (fd, remote_addr, remote_port))) {
      WEBC_UTIL_LOG ("[%s:%u] OOM error\n", remote_addr, remote_port);
      goto errorexit;
   }

   if ((pthread_attr_init (&attr))!=0) {
      WEBC_UTIL_LOG ("[%s:%u] Failed to initiliase thread attributes: %m\n",
                  remote_addr, remote_port);
      goto errorexit;
   }
   if ((pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED))!=0) {
      WEBC_UTIL_LOG ("[%s:%u] Failed to set the thread to detached: %m\n",
                  remote_addr, remote_port);
      goto errorexit;
   }
   if ((pthread_create (&thread, &attr, thread_func, args))!=0) {
      WEBC_UTIL_LOG ("[%s:%u] Failed to start thread\n", remote_addr, remote_port);
      goto errorexit;
   }


   error = false;

errorexit:
   if (error) {
      WEBC_UTIL_LOG ("Thread start failure: closing client fd %i\n", fd);
      close (fd);
      thread_args_del (args);
   }
   return !error;
}


bool webc_util_vsprintf (char **dst, size_t *dst_len, const char *fmts, va_list ap)
{
   va_list ac;

   va_copy (ac, ap);

   int nbytes = vsnprintf (NULL, 0, fmts, ac);

   va_end (ac);

   if (nbytes <= 0)
      return false;

   char *tmp = malloc (nbytes + 1);
   if (!tmp)
      return false;

   if ((vsnprintf (tmp, nbytes+1, fmts, ap))!=nbytes) {
      free (tmp);
      return false;
   }

   *dst = tmp;
   if (dst_len)
      *dst_len = nbytes;

   return true;
}

bool webc_util_sprintf (char **dst, size_t *dst_len, const char *fmts, ...)
{
   va_list ap;
   va_start (ap, fmts);
   bool ret = webc_util_vsprintf (dst, dst_len, fmts, ap);
   va_end (ap);
   return ret;
}

#if 1
// These two must be commented out if your linker fails with "multiple
// references" errors. Don't forget to comment them out in the util.h header
// as well.
int stricmp (const char *s1, const char *s2)
{
   while (*s1 && *s2) {
      int result = toupper (*s1++) - toupper (*s2++);
      if (result)
         return result;
   }
   return *s1 - *s2;
}

int strnicmp (const char *s1, const char *s2, size_t n)
{
   while (*s1 && *s2 && n--) {
      int result = toupper (*s1++) - toupper (*s2++);
      if (result)
         return result;
   }
   return *s1 - *s2;
}
#endif


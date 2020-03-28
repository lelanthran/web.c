
#include <string.h>
#include <stddef.h>
#include <stdarg.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include <pthread.h>

#include "resource.h"
#include "util.h"
#include "config.h"
#include "header.h"

static enum method_t get_rqst_method (const char *rqst_line)
{
   static const struct {
      const char *name;
      enum method_t method;
   } methods[] = {
      { "UNKNOWN",   method_UNKNOWN  },
      { "GET",       method_GET      },
      { "HEAD",      method_HEAD     },
      { "POST",      method_POST     },
      { "PUT",       method_PUT      },
      { "DELETE",    method_DELETE   },
      { "TRACE",     method_TRACE    },
      { "OPTIONS",   method_OPTIONS  },
      { "CONNECT",   method_CONNECT  },
      { "PATCH",     method_PATCH    },
   };

   for (size_t i=0; i<sizeof methods/sizeof methods[0]; i++) {
      if ((memcmp (methods[i].name, rqst_line, strlen (methods[i].name)))==0) {
         return methods[i].method;
      }
   }

   return method_UNKNOWN;
}

static enum http_version_t get_rqst_version (const char *rqst_line)
{
   static const struct {
      const char *name;
      enum http_version_t version;
   } versions[] = {
      { "UNKNOWN",      http_version_UNKNOWN  },
      { "XXXHTTP/0.9",  http_version_0_9      },
      { "HTTP/1.0",     http_version_1_0      },
      { "HTTP/1.1",     http_version_1_1      },
      { "XXXHTTP/2",    http_version_2_0      },
      { "XXXHTTP/3",    http_version_3_0      },
   };

   char *tmp = strstr (rqst_line, "HTTP/");
   if (!tmp)
      return http_version_UNKNOWN;

   for (size_t i=0; i<sizeof versions/sizeof versions[0]; i++) {
      if ((memcmp (versions[i].name, tmp, strlen (versions[i].name)))==0) {
         return versions[i].version;
      }
   }

   return http_version_UNKNOWN;
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
   return ret;
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

int create_listener (uint32_t portnum, int backlog)
{
   struct sockaddr_in addr;

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = INADDR_ANY;
   addr.sin_port = htons (portnum & 0xffff);
   int fd = -1;

   fd = socket (AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
   if (fd < 0) {
      UTIL_LOG ("socket() failed: %m\n");
      return -1;
   }
   int enable = 1;
   if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
      UTIL_LOG ("setsockopt(SO_REUSEADDR) failed, continuing");

   if (bind (fd, (struct sockaddr *)&addr, sizeof addr)!=0) {
      UTIL_LOG ("bind() failed: %m\n");
      close (fd);
      return -1;
   }
   if (listen (fd, backlog)!=0) {
      UTIL_LOG ("listen() failed: %m\n");
      close (fd);
      return -1;
   }
   return fd;
}


int accept_conn (int listenfd, size_t timeout,
                               char **remote_addr,
                               uint16_t *remote_port)
{
   struct sockaddr_in ret;
   socklen_t retlen = sizeof ret;
   int retval = -1;

   memset (&ret, 0xff, sizeof ret);

   fd_set fds;
   struct timeval tv = { (long int)timeout , 0 };
   FD_ZERO (&fds);
   FD_SET (listenfd, &fds);
   int r = select (listenfd + 1, &fds, &fds, &fds, &tv);
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
         UTIL_LOG ("OOM error\n");
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

const char *get_http_rspstr (int status)
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

   enum method_t method = 0;
   char *org_resource = NULL;
   char *resource = NULL;
   char *getvars = NULL;
   enum http_version_t version = 0;
   resource_handler_t *resource_handler = NULL;

   char *rqst_line = NULL;
   size_t rqst_line_len = 0;

   char *rqst_headers[MAX_HTTP_HEADERS];
   size_t rqst_header_lens[MAX_HTTP_HEADERS];

   header_t *rsp_headers = NULL;

   size_t i;

   memset (rqst_headers, 0, MAX_HTTP_HEADERS * sizeof rqst_headers[0]);
   memset (rqst_header_lens, 0, MAX_HTTP_HEADERS * sizeof rqst_header_lens[0]);

   if (!(rsp_headers = header_new ())) {
      THRD_LOG (args->remote_addr, args->remote_port,
                  "Failed to create header object\n");
      goto errorexit;
   }

   if (!(fd_read_line (args->fd, &rqst_line, &rqst_line_len))) {
      THRD_LOG (args->remote_addr, args->remote_port,
                "Malformed request line: [%s]. Aborting.\n", rqst_line);
      status = 400;
      goto errorexit;
   }

   THRD_LOG (args->remote_addr, args->remote_port, "REQUEST: [%s]\n",
             rqst_line);

   for (i=0; i<MAX_HTTP_HEADERS; i++) {
      if (!(fd_read_line (args->fd, &rqst_headers[i], &rqst_header_lens[i]))) {
         THRD_LOG (args->remote_addr, args->remote_port,
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
      THRD_LOG (args->remote_addr, args->remote_port,
                "Too many rqst_headers sent (%zu), ignoring the rest\n", i);
   }

#if 0
   THRD_LOG (args->remote_addr, args->remote_port, "Collected all rqst_headers\n");
   THRD_LOG (args->remote_addr, args->remote_port,
             "rqst: [%s]\n", rqst_line);

   for (size_t i=0; rqst_headers[i]; i++) {
      THRD_LOG (args->remote_addr, args->remote_port,
                "header: [%s]\n", rqst_headers[i]);
   }
#endif

   method = get_rqst_method (rqst_line);
   org_resource = get_rqst_resource (rqst_line);
   version = get_rqst_version (rqst_line);
   getvars = get_rqst_getvars (rqst_line);
   resource_handler = resource_handler_find (org_resource);

   if (!method || !org_resource || !version || !resource_handler) {
      THRD_LOG (args->remote_addr, args->remote_port,
                "Unrecognised method, version or resource [%s]\n",
                 rqst_line);
      status = 400;
      goto errorexit;
   }

   if ((strstr (org_resource, ".."))!=NULL) {
      THRD_LOG (args->remote_addr, args->remote_port,
                "Attempt to access parent directory [%s]\n",
                 rqst_line);
      status = 403;
      goto errorexit;
   }

   resource = (org_resource[0]=='/') ? &org_resource[1] : org_resource;

   status = resource_handler (args->fd, args->remote_addr, args->remote_port,
                              method, version, resource,
                              rqst_headers, rsp_headers,
                              getvars);

   THRD_LOG (args->remote_addr, args->remote_port, "rqst:%s rsp:%s",
               rqst_line, get_http_rspstr (status));

errorexit:

   if (!status || status!=200) {
      rsp_line = get_http_rspstr (status);
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
   header_del (rsp_headers);

   shutdown (args->fd, SHUT_RDWR);
   close (args->fd);

   THRD_LOG (args->remote_addr, args->remote_port, "Ending thread\n");
   thread_args_del (args);

   return NULL;
}

bool handle_conn (int fd, char *remote_addr, uint16_t remote_port)
{
   bool error = true;

   pthread_attr_t attr;
   pthread_t thread;
   struct thread_args_t *args = NULL;

   if (!(args = thread_args_new (fd, remote_addr, remote_port))) {
      UTIL_LOG ("[%s:%u] OOM error\n", remote_addr, remote_port);
      goto errorexit;
   }

   if ((pthread_attr_init (&attr))!=0) {
      UTIL_LOG ("[%s:%u] Failed to initiliase thread attributes: %m\n",
                  remote_addr, remote_port);
      goto errorexit;
   }
   if ((pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED))!=0) {
      UTIL_LOG ("[%s:%u] Failed to set the thread to detached: %m\n",
                  remote_addr, remote_port);
      goto errorexit;
   }
   if ((pthread_create (&thread, &attr, thread_func, args))!=0) {
      UTIL_LOG ("[%s:%u] Failed to start thread\n", remote_addr, remote_port);
      goto errorexit;
   }


   error = false;

errorexit:
   if (error) {
      UTIL_LOG ("Thread start failure: closing client fd %i\n", fd);
      close (fd);
      thread_args_del (args);
   }
   return !error;
}


bool util_vsprintf (char **dst, size_t *dst_len, const char *fmts, va_list ap)
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

bool util_sprintf (char **dst, size_t *dst_len, const char *fmts, ...)
{
   va_list ap;
   va_start (ap, fmts);
   bool ret = util_vsprintf (dst, dst_len, fmts, ap);
   va_end (ap);
   return ret;
}


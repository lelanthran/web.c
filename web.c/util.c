
#include <string.h>
#include <stddef.h>


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

#define THRD_LOG(addr,port,...)      do {\
      fprintf (stderr, "%s:%d: [%s:%u] ", __FILE__, __LINE__, addr, port);\
      fprintf (stderr, __VA_ARGS__);\
} while (0)


const char *get_http_rspstr (int status)
{
   static const struct {
      int status;
      const char *string;
   } statuses[] = {
      {  100,  "HTTP/1.1 100 Continue"                              },
      {  101,  "HTTP/1.1 101 Switching Protocols"                   },
      {  102,  "HTTP/1.1 102 Processing"                            },
      {  103,  "HTTP/1.1 103 Checkpoint"                            },
      {  103,  "HTTP/1.1 103 Early Hints"                           },
      {  200,  "HTTP/1.1 200 OK"                                    },
      {  201,  "HTTP/1.1 201 Created"                               },
      {  202,  "HTTP/1.1 202 Accepted"                              },
      {  203,  "HTTP/1.1 203 Non-Authoritative Information"         },
      {  204,  "HTTP/1.1 204 No Content"                            },
      {  205,  "HTTP/1.1 205 Reset Content"                         },
      {  206,  "HTTP/1.1 206 Partial Content"                       },
      {  207,  "HTTP/1.1 207 Multi-Status"                          },
      {  208,  "HTTP/1.1 208 Already Reported"                      },
      {  218,  "HTTP/1.1 218 This is fine"                          },
      {  226,  "HTTP/1.1 226 IM Used"                               },
      {  300,  "HTTP/1.1 300 Multiple Choices"                      },
      {  301,  "HTTP/1.1 301 Moved Permanently"                     },
      {  302,  "HTTP/1.1 302 Found"                                 },
      {  303,  "HTTP/1.1 303 See Other"                             },
      {  304,  "HTTP/1.1 304 Not Modified"                          },
      {  305,  "HTTP/1.1 305 Use Proxy"                             },
      {  306,  "HTTP/1.1 306 Switch Proxy"                          },
      {  307,  "HTTP/1.1 307 Temporary Redirect"                    },
      {  308,  "HTTP/1.1 308 Permanent Redirect"                    },
      {  400,  "HTTP/1.1 400 Bad Request"                           },
      {  401,  "HTTP/1.1 401 Unauthorized"                          },
      {  402,  "HTTP/1.1 402 Payment Required"                      },
      {  403,  "HTTP/1.1 403 Forbidden"                             },
      {  404,  "HTTP/1.1 404 Not Found"                             },
      {  405,  "HTTP/1.1 405 Method Not Allowed"                    },
      {  406,  "HTTP/1.1 406 Not Acceptable"                        },
      {  407,  "HTTP/1.1 407 Proxy Authentication Required "        },
      {  408,  "HTTP/1.1 408 Request Timeout"                       },
      {  409,  "HTTP/1.1 409 Conflict"                              },
      {  410,  "HTTP/1.1 410 Gone"                                  },
      {  411,  "HTTP/1.1 411 Length Required"                       },
      {  412,  "HTTP/1.1 412 Precondition Failed "                  },
      {  413,  "HTTP/1.1 413 Payload Too Large "                    },
      {  414,  "HTTP/1.1 414 URI Too Long "                         },
      {  415,  "HTTP/1.1 415 Unsupported Media Type "               },
      {  416,  "HTTP/1.1 416 Range Not Satisfiable "                },
      {  417,  "HTTP/1.1 417 Expectation Failed"                    },
      {  418,  "HTTP/1.1 418 I'm a teapot "                         },
      {  419,  "HTTP/1.1 419 Page Expired "                         },
      {  420,  "HTTP/1.1 420 Enhance Your Calm "                    },
      {  420,  "HTTP/1.1 420 Method Failure "                       },
      {  421,  "HTTP/1.1 421 Misdirected Request "                  },
      {  422,  "HTTP/1.1 422 Unprocessable Entity "                 },
      {  423,  "HTTP/1.1 423 Locked "                               },
      {  424,  "HTTP/1.1 424 Failed Dependency "                    },
      {  425,  "HTTP/1.1 425 Too Early "                            },
      {  426,  "HTTP/1.1 426 Upgrade Required"                      },
      {  428,  "HTTP/1.1 428 Precondition Required "                },
      {  429,  "HTTP/1.1 429 Too Many Requests "                    },
      {  430,  "HTTP/1.1 430 Request Header Fields Too Large "      },
      {  431,  "HTTP/1.1 431 Request Header Fields Too Large "      },
      {  440,  "HTTP/1.1 440 Login Time-out"                        },
      {  444,  "HTTP/1.1 444 No Response"                           },
      {  449,  "HTTP/1.1 449 Retry With"                            },
      {  450,  "HTTP/1.1 450 Blocked by Windows Parental Controls " },
      {  451,  "HTTP/1.1 451 Redirect"                              },
      {  451,  "HTTP/1.1 451 Unavailable For Legal Reasons "        },
      {  494,  "HTTP/1.1 494 Request header too large"              },
      {  495,  "HTTP/1.1 495 SSL Certificate Error"                 },
      {  496,  "HTTP/1.1 496 SSL Certificate Required"              },
      {  497,  "HTTP/1.1 497 HTTP Request Sent to HTTPS Port"       },
      {  498,  "HTTP/1.1 498 Invalid Token "                        },
      {  499,  "HTTP/1.1 499 Client Closed Request"                 },
      {  499,  "HTTP/1.1 499 Token Required "                       },
      {  500,  "HTTP/1.1 500 Internal Server Error"                 },
      {  501,  "HTTP/1.1 501 Not Implemented"                       },
      {  502,  "HTTP/1.1 502 Bad Gateway"                           },
      {  503,  "HTTP/1.1 503 Service Unavailable"                   },
      {  504,  "HTTP/1.1 504 Gateway Timeout"                       },
      {  505,  "HTTP/1.1 505 HTTP Version Not Supported"            },
      {  506,  "HTTP/1.1 506 Variant Also Negotiates "              },
      {  507,  "HTTP/1.1 507 Insufficient Storage "                 },
      {  508,  "HTTP/1.1 508 Loop Detected "                        },
      {  509,  "HTTP/1.1 509 Bandwidth Limit Exceeded "             },
      {  510,  "HTTP/1.1 510 Not Extended "                         },
      {  511,  "HTTP/1.1 511 Network Authentication Required "      },
      {  520,  "HTTP/1.1 520 Web Server Returned an Unknown Error"  },
      {  521,  "HTTP/1.1 521 Web Server Is Down"                    },
      {  522,  "HTTP/1.1 522 Connection Timed Out"                  },
      {  523,  "HTTP/1.1 523 Origin Is Unreachable"                 },
      {  524,  "HTTP/1.1 524 A Timeout Occurred"                    },
      {  525,  "HTTP/1.1 525 SSL Handshake Failed"                  },
      {  526,  "HTTP/1.1 526 Invalid SSL Certificate"               },
      {  527,  "HTTP/1.1 527 Railgun Error"                         },
      {  529,  "HTTP/1.1 529 Site is overloaded"                    },
      {  530,  "HTTP/1.1 530 Site is frozen"                        },
      {  598,  "HTTP/1.1 598  Network read timeout error"    },
   };

   for (size_t i=0; i<sizeof statuses/sizeof statuses[0]; i++) {
      if (statuses[i].status == status)
         return statuses[i].string;
   }

   return "HTTP/1.1 500 Internal Server Error";
}

static void *thread_func (void *ta)
{
   int status = 500;
   const char *rsp_line = NULL;

   struct thread_args_t *args = ta;

   enum method_t method = 0;
   char *org_resource = NULL;
   char *resource = NULL;
   enum http_version_t version = 0;
   resource_handler_t *resource_handler = NULL;

   char *rqst_line = NULL;
   size_t rqst_line_len = 0;

   char *headers[MAX_HTTP_HEADERS];
   size_t header_lens[MAX_HTTP_HEADERS];

   size_t i;

   THRD_LOG (args->remote_addr, args->remote_port, "Received connection\n");

   memset (headers, 0, MAX_HTTP_HEADERS * sizeof headers[0]);
   memset (header_lens, 0, MAX_HTTP_HEADERS * sizeof header_lens[0]);

   if (!(fd_read_line (args->fd, &rqst_line, &rqst_line_len))) {
      THRD_LOG (args->remote_addr, args->remote_port,
                "Malformed request line: [%s]. Aborting.\n", rqst_line);
      status = 400;
      goto errorexit;
   }

   THRD_LOG (args->remote_addr, args->remote_port, "Read header\n");

   for (i=0; i<MAX_HTTP_HEADERS; i++) {
      if (!(fd_read_line (args->fd, &headers[i], &header_lens[i]))) {
         THRD_LOG (args->remote_addr, args->remote_port,
                   "Unexpected end of headers");
         status = 400;
         goto errorexit;
      }
      if (header_lens[i]==0) {
         // Reached the empty line, everything after this is the message body
         break;
      }
   }

   if (i >= MAX_HTTP_HEADERS) {
      THRD_LOG (args->remote_addr, args->remote_port,
                "Too many headers sent (%zu), ignoring the rest\n", i);
   }

#if 0
   THRD_LOG (args->remote_addr, args->remote_port, "Collected all headers\n");
   THRD_LOG (args->remote_addr, args->remote_port,
             "rqst: [%s]\n", rqst_line);

   for (size_t i=0; headers[i]; i++) {
      THRD_LOG (args->remote_addr, args->remote_port,
                "header: [%s]\n", headers[i]);
   }
#endif

   method = get_rqst_method (rqst_line);
   org_resource = get_rqst_resource (rqst_line);
   version = get_rqst_version (rqst_line);
   resource_handler = resource_handler_find (org_resource);

   if (!method || !org_resource || !version || !resource_handler) {
      THRD_LOG (args->remote_addr, args->remote_port,
                "Unrecognised method, version or resource [%s]\n",
                 rqst_line);
      status = 400;
      goto errorexit;
   }

   resource = (org_resource[0]=='/') ? &org_resource[1] : org_resource;

   status = resource_handler (args->fd, args->remote_addr, args->remote_port,
                              method, version, resource, headers);

   THRD_LOG (args->remote_addr, args->remote_port, "[%i:%s:%i]\n",
               method, resource, version);


errorexit:
   rsp_line = get_http_rspstr (status);

   rsp_line = rsp_line ? rsp_line : "HTTP/1.1 500 Internal Server Error";

   if (status && rsp_line) {
      write (args->fd, rsp_line, strlen (rsp_line));
   } else {
      THRD_LOG (args->remote_addr, args->remote_port,
                "No rspstr for status %i\n", status);
   }

   free (rqst_line);
   free (org_resource);

   for (i=0; i<MAX_HTTP_HEADERS; i++) {
      free (headers[i]);
   }

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


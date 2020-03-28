
#ifndef H_UTIL
#define H_UTIL

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#define UTIL_LOG(...)      do {\
      fprintf (stderr, "%s:%d: ", __FILE__, __LINE__);\
      fprintf (stderr, __VA_ARGS__);\
} while (0)

#define THRD_LOG(addr,port,...)      do {\
      fprintf (stderr, "%s:%d: [%s:%u] ", __FILE__, __LINE__, addr, port);\
      fprintf (stderr, __VA_ARGS__);\
} while (0)



enum method_t {
   method_UNKNOWN = 0,
   method_GET,
   method_HEAD,
   method_POST,
   method_PUT,
   method_DELETE,
   method_TRACE,
   method_OPTIONS,
   method_CONNECT,
   method_PATCH
};

enum http_version_t {
   http_version_UNKNOWN = 0,
   http_version_0_9,
   http_version_1_0,
   http_version_1_1,
   http_version_2_0,
   http_version_3_0,
};


#ifdef __cplusplus
extern "C" {
#endif

   /* ******************************************************************* *
    * Utility functions for callers to use
    */

   // Print the formatted string into dst, which the caller must free. If
   // dst_len is not NULL it is populated with the length of the resulting
   // string.
   bool util_sprintf (char **dst, size_t *dst_len, const char *fmts, ...);
   bool util_vsprintf (char **dst, size_t *dst_len, const char *fmts, va_list ap);

   /* ******************************************************************* *
    * Functions used by the web-server itself. Having them in the .so file
    * reduces the memory load when multiple instances of the web server is
    * running.
    */
   int create_listener (uint32_t portnum, int backlog);

   int accept_conn (int listenfd, size_t timeout,
                                  char **remote_addr,
                                  uint16_t *remote_port);

   bool handle_conn (int fd, char *remote_addr, uint16_t remote_port);

   const char *get_http_rspstr (int status);

#ifdef __cplusplus
};
#endif

#endif


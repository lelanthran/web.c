
#ifndef H_UTIL
#define H_UTIL

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define UTIL_LOG(...)      do {\
      fprintf (stderr, "%s:%d: ", __FILE__, __LINE__);\
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


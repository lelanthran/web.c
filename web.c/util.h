
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

#ifdef __cplusplus
extern "C" {
#endif

   int create_listener (uint32_t portnum, int backlog);

   int accept_conn (int listenfd, size_t timeout,
                                  char **remote_addr,
                                  uint16_t *remote_port);

   bool start_webserver (int fd, char *remote_addr, uint16_t remote_port);

#ifdef __cplusplus
};
#endif

#endif


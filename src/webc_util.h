
#ifndef H_UTIL
#define H_UTIL

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

#define WEBC_UTIL_LOG(...)      do {\
      fprintf (stderr, "%s:%d: ", __FILE__, __LINE__);\
      fprintf (stderr, __VA_ARGS__);\
} while (0)

#define WEBC_THRD_LOG(addr,port,...)      do {\
      fprintf (stderr, "%s:%d: [%s:%u] ", __FILE__, __LINE__, addr, port);\
      fprintf (stderr, __VA_ARGS__);\
} while (0)

#define WEBC_TS_LOG(...)        do {\
   char template[] = "YYYYMMDDhhmmss";\
   time_t now = time (NULL);\
   struct tm *time_fields = localtime (&now);\
   if (!time_fields) {\
      WEBC_UTIL_LOG ("Failed to get local time: %m\n");\
   } else {\
      snprintf (template, sizeof template, "%04i"\
                                           "%02i"\
                                           "%02i"\
                                           "%02i"\
                                           "%02i"\
                                           "%02i",\
                                           time_fields->tm_year + 1900,\
                                           time_fields->tm_mon + 1,\
                                           time_fields->tm_mday,\
                                           time_fields->tm_hour,\
                                           time_fields->tm_min,\
                                           time_fields->tm_sec);\
      fprintf (stderr, "%s: ", template);\
      fprintf (stderr, __VA_ARGS__);\
   }\
} while (0)



enum webc_method_t {
   webc_method_UNKNOWN = 0,
   webc_method_GET,
   webc_method_HEAD,
   webc_method_POST,
   webc_method_PUT,
   webc_method_DELETE,
   webc_method_TRACE,
   webc_method_OPTIONS,
   webc_method_CONNECT,
   webc_method_PATCH
};

enum webc_http_version_t {
   webc_http_version_UNKNOWN = 0,
   webc_http_version_0_9,
   webc_http_version_1_0,
   webc_http_version_1_1,
   webc_http_version_2_0,
   webc_http_version_3_0,
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
   bool webc_util_sprintf (char **dst, size_t *dst_len, const char *fmts, ...);
   bool webc_util_vsprintf (char **dst, size_t *dst_len, const char *fmts, va_list ap);

#if 1
   // These two must be commented out if your linker fails with "multiple
   // references" errors. Don't forget to comment them out in the util.c
   // implementation as well.
   int stricmp (const char *s1, const char *s2);
   int strnicmp (const char *s1, const char *s2, size_t n);
#endif


   /* ******************************************************************* *
    * Functions used by the web-server itself. Having them in the .so file
    * reduces the memory load when multiple instances of the web server is
    * running.
    */
   int webc_create_listener (uint32_t portnum, int backlog);

   int webc_accept_conn (int listenfd, size_t timeout,
                                  char **remote_addr,
                                  uint16_t *remote_port);

   bool webc_handle_conn (int fd, char *remote_addr, uint16_t remote_port);

   const char *webc_get_http_rspstr (int status);


#ifdef __cplusplus
};
#endif

#endif


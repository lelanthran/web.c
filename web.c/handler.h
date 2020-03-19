
#ifndef H_HANDLER
#define H_HANDLER

#include <stdbool.h>
#include <stdint.h>

#include "resource.h"

#define HANDLER(x)   int x (int                    fd,            \
                            char                  *remote_addr,   \
                            uint16_t               remote_port,   \
                            enum method_t          method,        \
                            enum http_version_t    version,       \
                            const char            *resource,      \
                            char                 **headers,       \
                            char                  *vars)

#ifdef __cplusplus
extern "C" {
#endif


   HANDLER (handler_static_file);
   HANDLER (handler_html);
   HANDLER (handler_none);
   HANDLER (handler_dir);
   HANDLER (handler_dirlist);

#ifdef __cplusplus
};
#endif

#endif

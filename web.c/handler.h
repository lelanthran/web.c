
#ifndef H_HANDLER
#define H_HANDLER

#include <stdbool.h>
#include <stdint.h>

#include "resource.h"

#ifdef __cplusplus
extern "C" {
#endif

   bool handler_static_file (int                    fd,
                             char                  *remote_addr,
                             uint16_t               remote_port,
                             enum method_t          method,
                             enum http_version_t    version,
                             const char            *resource,
                             char                 **headers);


#ifdef __cplusplus
};
#endif

#endif

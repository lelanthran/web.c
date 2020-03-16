
#ifndef H_RESOURCE
#define H_RESOURCE

#include <stdint.h>

#include "util.h"

typedef bool (resource_handler_t) (int                    fd,
                                   char                  *remote_addr,
                                   uint16_t               remote_port,
                                   enum method_t          method,
                                   enum http_version_t    version,
                                   const char            *resource,
                                   char                 **headers);

#ifdef __cplusplus
extern "C" {
#endif

   resource_handler_t *resource_handler_find (const char *resource);


#ifdef __cplusplus
};
#endif

#endif


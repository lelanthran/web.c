
#ifndef H_RESOURCE
#define H_RESOURCE

#include <stdint.h>
#include <stdbool.h>

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

   // Before _add() is called the _lock() function must be called. After all
   // the _add() calls the caller must call the _unlock() function.
   bool resource_global_handler_lock (void);
   bool resource_global_handler_add (const char *pattern, resource_handler_t *handler);
   bool resource_global_handler_unlock (void);

   resource_handler_t *resource_handler_find (const char *resource);



#ifdef __cplusplus
};
#endif

#endif



#ifndef H_RESOURCE
#define H_RESOURCE

#include <stdint.h>
#include <stdbool.h>

#include "webc_util.h"
#include "webc_header.h"

enum webc_pattern_type_t {
   pattern_SUFFIX,
   pattern_PREFIX,
   pattern_EXACT
};

typedef int (webc_resource_handler_t) (int                        fd,
                                       char                      *remote_addr,
                                       uint16_t                   remote_port,
                                       enum webc_method_t         method,
                                       enum webc_http_version_t   version,
                                       const char                *resource,
                                       char                     **rqst_headers,
                                       webc_header_t             *rsp_headers,
                                       char                      *vars);

#ifdef __cplusplus
extern "C" {
#endif

   // Before _add() is called the _lock() function must be called. After all
   // the _add() calls the caller must call the _unlock() function.
   bool webc_resource_global_handler_lock (void);
   bool webc_resource_global_handler_add (const char *name,
                                          const char *pattern,
                                          enum webc_pattern_type_t type,
                                          webc_resource_handler_t *handler);
   bool webc_resource_global_handler_unlock (void);

   webc_resource_handler_t *webc_resource_handler_find (const char *resource);



#ifdef __cplusplus
};
#endif

#endif


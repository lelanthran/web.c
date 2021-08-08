
#ifndef H_HANDLER
#define H_HANDLER

#include <stdbool.h>
#include <stdint.h>

#include "webc_resource.h"

#define WEBC_HANDLER(x)   int x (int                       fd,            \
                                 char                     *remote_addr,   \
                                 uint16_t                  remote_port,   \
                                 enum webc_method_t        method,        \
                                 enum webc_http_version_t  version,       \
                                 const char               *resource,      \
                                 char                    **rqst_headers,  \
                                 webc_header_t            *rsp_headers,   \
                                 char                     *vars)

#ifdef __cplusplus
extern "C" {
#endif


   WEBC_HANDLER (webc_handler_static_file);
   WEBC_HANDLER (webc_handler_html);
   WEBC_HANDLER (webc_handler_none);
   WEBC_HANDLER (webc_handler_dir);
   WEBC_HANDLER (webc_handler_dirlist);

#undef WEBC_HANDLER

#ifdef __cplusplus
};
#endif

#endif

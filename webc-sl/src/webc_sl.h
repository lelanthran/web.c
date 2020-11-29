
#ifndef H_WEBC_SL
#define H_WEBC_SL

#include <stdint.h>
#include <stdlib.h>


#include "webc_header.h"
#include "webc_util.h"

#ifdef __cplusplus
extern "C" {
#endif

   int load_all_cline_opts (int argc, char **argv);
   const char *webc_getenv (const char *name);
   int webc_setenv (const char *name, const char *value);

#define WEBC_HANDLER(x)   int x (int                       fd,            \
                                 char                     *remote_addr,   \
                                 uint16_t                  remote_port,   \
                                 enum webc_method_t        method,        \
                                 enum webc_http_version_t  version,       \
                                 const char               *resource,      \
                                 char                    **rqst_headers,  \
                                 webc_header_t            *rsp_headers,   \
                                 char                     *vars)

   WEBC_HANDLER (webc_sl_handler_auth);
   WEBC_HANDLER (webc_sl_handler_call_sp);
   WEBC_HANDLER (webc_sl_handler_call_native);
   WEBC_HANDLER (webc_sl_handler_call_fileAPI);

#undef WEBC_HANDLER

#ifdef __cplusplus
};
#endif


#endif

#define _POSIX_C_SOURCE    200809L

#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include "webc_util.h"
#include "webc_resource.h"
#include "webc_handler.h"
#include "webc_web-add.h"

static int app_handler (int                      fd,
                        char                    *remote_addr,
                        uint16_t                 remote_port,
                        enum webc_method_t       method,
                        enum webc_http_version_t version,
                        const char              *resource,
                        char                   **rqst_headers,
                        webc_header_t           *rsp_headers,
                        char                    *vars)
{
   (void)remote_addr;
   (void)remote_port;
   (void)method;
   (void)version;
   (void)rqst_headers;

   webc_header_set (rsp_headers, webc_header_CONTENT_TYPE, "text/html");
   write (fd, webc_get_http_rspstr (200), strlen (webc_get_http_rspstr (200)));
   webc_header_write (rsp_headers, fd);

   dprintf (fd, "<html>\n\t<body>\n\t\t<pre>%s\n%s</pre>\n\t</body>\n</html>\n",
                resource, vars);

   return 200;
}

bool web_add_init (void)
{
   /* All the initialisation you want to do on startup must go here. On
    * success return true. On failure return false.
    */

   return true;
}

bool web_add_load_handlers (void)
{
   /* All the handlers you want to load must go here. On success return
    * true. On failure return false.
    */

   if (!(webc_resource_global_handler_add ("app_handler", "/myapp", pattern_PREFIX,
                                      app_handler))) {
      WEBC_UTIL_LOG ("Failed to add handler [%s]\n", "/myapp");
      return false;
   }

   return true;
}


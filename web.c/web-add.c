#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include "util.h"
#include "resource.h"
#include "handler.h"

#include "web-add.h"

static int app_handler (int                    fd,
                        char                  *remote_addr,
                        uint16_t               remote_port,
                        enum method_t          method,
                        enum http_version_t    version,
                        const char            *resource,
                        char                 **rqst_headers,
                        header_t              *rsp_headers,
                        char                  *vars)
{
   (void)remote_addr;
   (void)remote_port;
   (void)method;
   (void)version;
   (void)rqst_headers;

   header_set (rsp_headers, header_CONTENT_TYPE, "text/html");
   write (fd, get_http_rspstr (200), strlen (get_http_rspstr (200)));
   header_write (rsp_headers, fd);

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

   if (!(resource_global_handler_add ("app_handler", "/myapp", pattern_PREFIX,
                                      app_handler))) {
      UTIL_LOG ("Failed to add handler [%s]\n", "/myapp");
      return false;
   }

   return true;
}


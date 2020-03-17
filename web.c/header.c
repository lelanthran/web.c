#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "header.h"

struct header_t {
   char **fields;
};

header_t *header_new (void)
{
   return calloc (1, sizeof (header_t));
}

void header_del (header_t *header)
{
   if (header) {
      for (size_t i=0; header->fields[i]; i++) {
         free (header->fields[i]);
      }
      free (header);
   }
}

static const char *find_namestring (enum header_name_t name)
{
   static const struct {
      enum header_name_t name;
      const char *sname;
   } names[] = {

{ header_ACCESS_CONTROL_ALLOW_ORIGIN,      "Access-Control-Allow-Origin"      },
{ header_ACCESS_CONTROL_ALLOW_CREDENTIALS, "Access-Control-Allow-Credentials" },
{ header_ACCESS_CONTROL_EXPOSE_HEADERS,    "Access-Control-Expose-Headers"    },
{ header_ACCESS_CONTROL_MAX_AGE,           "Access-Control-Max-Age"           },
{ header_ACCESS_CONTROL_ALLOW_METHODS,     "Access-Control-Allow-Methods"     },
{ header_ACCESS_CONTROL_ALLOW_HEADERS,     "Access-Control-Allow-Headers"     },
{ header_ACCEPT_PATCH,                     "Accept-Patch"                     },
{ header_ACCEPT_RANGES,                    "Accept-Ranges"                    },
{ header_AGE,                              "Age"                              },
{ header_ALLOW,                            "Allow"                            },
{ header_ALT_SVC,                          "Alt-Svc"                          },
{ header_CACHE_CONTROL,                    "Cache-Control"                    },
{ header_CONNECTION,                       "Connection"                       },
{ header_CONTENT_DISPOSITION,              "Content-Disposition"              },
{ header_CONTENT_ENCODING,                 "Content-Encoding"                 },
{ header_CONTENT_LANGUAGE,                 "Content-Language"                 },
{ header_CONTENT_LENGTH,                   "Content-Length"                   },
{ header_CONTENT_LOCATION,                 "Content-Location"                 },
{ header_CONTENT_MD5,                      "Content-MD5"                      },
{ header_CONTENT_RANGE,                    "Content-Range"                    },
{ header_CONTENT_TYPE,                     "Content-Type"                     },
{ header_DATE,                             "Date"                             },
{ header_DELTA_BASE,                       "Delta-Base"                       },
{ header_ETAG,                             "ETag"                             },
{ header_EXPIRES,                          "Expires"                          },
{ header_IM,                               "IM"                               },
{ header_LAST_MODIFIED,                    "Last-Modified"                    },
{ header_LINK,                             "Link"                             },
{ header_LOCATION,                         "Location"                         },
{ header_P3P,                              "P3P"                              },
{ header_PRAGMA,                           "Pragma"                           },
{ header_PROXY_AUTHENTICATE,               "Proxy-Authenticate"               },
{ header_PUBLIC_KEY_PINS,                  "Public-Key-Pins"                  },
{ header_RETRY_AFTER,                      "Retry-After"                      },
{ header_SERVER,                           "Server"                           },
{ header_SET_COOKIE,                       "Set-Cookie"                       },
{ header_STRICT_TRANSPORT_SECURITY,        "Strict-Transport-Security"        },
{ header_TRAILER,                          "Trailer"                          },
{ header_TRANSFER_ENCODING,                "Transfer-Encoding"                },
{ header_TK,                               "Tk"                               },
{ header_UPGRADE,                          "Upgrade"                          },
{ header_VARY,                             "Vary"                             },
{ header_VIA,                              "Via"                              },
{ header_WARNING,                          "Warning"                          },
{ header_WWW_AUTHENTICATE,                 "WWW-Authenticate"                 },
{ header_X_FRAME_OPTIONS,                  "X-Frame-Options"                  },
{ header_CONTENT_SECURITY_POLICY,          "Content-Security-Policy"          },
{ header_X_CONTENT_SECURITY_POLICY,        "X-Content-Security-Policy"        },
{ header_X_WEBKIT_CSP,                     "X-WebKit-CSP"                     },
{ header_REFRESH,                          "Refresh"                          },
{ header_STATUS,                           "Status"                           },
{ header_TIMING_ALLOW_ORIGIN,              "Timing-Allow-Origin"              },
{ header_X_CONTENT_DURATION,               "X-Content-Duration"               },
{ header_X_CONTENT_TYPE_OPTIONS,           "X-Content-Type-Options"           },
{ header_X_POWERED_BY,                     "X-Powered-By"                     },
{ header_X_REQUEST_ID,                     "X-Request-ID"                     },
{ header_X_CORRELATION_ID,                 "X-Correlation-ID"                 },
{ header_X_UA_COMPATIBLE,                  "X-UA-Compatible"                  },
{ header_X_XSS_PROTECTION,                 "X-XSS-Protection"                 },
   };

   for (size_t i=0; i<sizeof names/sizeof names[0]; i++) {
      if (names[i].name == name) {
         return names[i].sname;
      }
   }
   return NULL;
}

static char *create_header_field (const char *name, const char *value)
{
   size_t newlen = strlen (name) + 2 + strlen (value) + 2;
   char *ret = malloc (newlen + 1);
   if (!ret)
      return false;
   strcpy (ret, name);
   strcat (ret, ": ");
   strcat (ret, value);
   strcat (ret, "\r\n");
   return ret;
}

bool header_set (header_t *header, enum header_name_t name, const char *value)
{
   const char *sname = find_namestring (name);

   if (!header || !value || !sname)
      return false;

   size_t name_len = strlen (sname);
   for (size_t i=0; header->fields[i]; i++) {
      if ((strncmp (header->fields[i], sname, name_len))==0) {
         char *tmp = create_header_field (sname, value);
         if (!tmp)
            return false;

         free (header->fields[i]);
         header->fields[i] = tmp;
      }
   }
   return header_add (header, name, value);
}

bool header_add (header_t *header, enum header_name_t name, const char *value)
{
   const char *sname = find_namestring (name);

   if (!header || !value || !sname)
      return false;

   size_t nfields;
   for (nfields = 0; header->fields && header->fields[nfields]; nfields++)
      ;

   char **tmp = realloc (header->fields, (nfields + 2) * sizeof *tmp);
   if (!tmp)
      return false;

   header->fields = tmp;

   if (!(header->fields[nfields] = create_header_field (sname, value)))
      return false;

   header->fields[nfields + 1] = NULL;

   return true;
}

bool header_clear (header_t *header, enum header_name_t name)
{
   const char *sname = find_namestring (name);

   if (!header || !sname)
      return false;

   size_t name_len = strlen (sname);
   for (size_t i=0; header->fields[i]; i++) {
      if ((strncmp (header->fields[i], sname, name_len))==0) {
         char *tmp = strdup ("");
         if (!tmp)
            return false;

         free (header->fields[i]);
         header->fields[i] = tmp;
      }
   }
   return true;
}



bool header_write (header_t *header, int fd)
{
   if (!header || fd < 0)
      return false;

   for (size_t i=0; header->fields[i]; i++) {
      size_t slen = strlen (header->fields[i]);
      size_t nbytes = write (fd, header->fields[i], slen);
      if (nbytes!=slen)
         return false;
   }

   if ((write (fd, "\r\n", 2))!=2)
      return false;

   return true;
}



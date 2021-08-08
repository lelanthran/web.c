#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "webc_header.h"
#include "webc_util.h"

struct webc_header_t {
   char **fields;
};

webc_header_t *webc_header_new (void)
{
   return calloc (1, sizeof (webc_header_t));
}

void webc_header_del (webc_header_t *header)
{
   if (header) {
      for (size_t i=0; header->fields && header->fields[i]; i++) {
         free (header->fields[i]);
      }
      free (header->fields);
      free (header);
   }
}

static const char *find_namestring (enum webc_header_name_t name)
{
   static const struct {
      enum webc_header_name_t name;
      const char *sname;
   } names[] = {

{ webc_header_ACCESS_CONTROL_ALLOW_ORIGIN,      "Access-Control-Allow-Origin"      },
{ webc_header_ACCESS_CONTROL_ALLOW_CREDENTIALS, "Access-Control-Allow-Credentials" },
{ webc_header_ACCESS_CONTROL_EXPOSE_HEADERS,    "Access-Control-Expose-Headers"    },
{ webc_header_ACCESS_CONTROL_MAX_AGE,           "Access-Control-Max-Age"           },
{ webc_header_ACCESS_CONTROL_ALLOW_METHODS,     "Access-Control-Allow-Methods"     },
{ webc_header_ACCESS_CONTROL_ALLOW_HEADERS,     "Access-Control-Allow-Headers"     },
{ webc_header_ACCEPT_PATCH,                     "Accept-Patch"                     },
{ webc_header_ACCEPT_RANGES,                    "Accept-Ranges"                    },
{ webc_header_AGE,                              "Age"                              },
{ webc_header_ALLOW,                            "Allow"                            },
{ webc_header_ALT_SVC,                          "Alt-Svc"                          },
{ webc_header_CACHE_CONTROL,                    "Cache-Control"                    },
{ webc_header_CONNECTION,                       "Connection"                       },
{ webc_header_CONTENT_DISPOSITION,              "Content-Disposition"              },
{ webc_header_CONTENT_ENCODING,                 "Content-Encoding"                 },
{ webc_header_CONTENT_LANGUAGE,                 "Content-Language"                 },
{ webc_header_CONTENT_LENGTH,                   "Content-Length"                   },
{ webc_header_CONTENT_LOCATION,                 "Content-Location"                 },
{ webc_header_CONTENT_MD5,                      "Content-MD5"                      },
{ webc_header_CONTENT_RANGE,                    "Content-Range"                    },
{ webc_header_CONTENT_TYPE,                     "Content-Type"                     },
{ webc_header_DATE,                             "Date"                             },
{ webc_header_DELTA_BASE,                       "Delta-Base"                       },
{ webc_header_ETAG,                             "ETag"                             },
{ webc_header_EXPIRES,                          "Expires"                          },
{ webc_header_IM,                               "IM"                               },
{ webc_header_LAST_MODIFIED,                    "Last-Modified"                    },
{ webc_header_LINK,                             "Link"                             },
{ webc_header_LOCATION,                         "Location"                         },
{ webc_header_P3P,                              "P3P"                              },
{ webc_header_PRAGMA,                           "Pragma"                           },
{ webc_header_PROXY_AUTHENTICATE,               "Proxy-Authenticate"               },
{ webc_header_PUBLIC_KEY_PINS,                  "Public-Key-Pins"                  },
{ webc_header_RETRY_AFTER,                      "Retry-After"                      },
{ webc_header_SERVER,                           "Server"                           },
{ webc_header_SET_COOKIE,                       "Set-Cookie"                       },
{ webc_header_STRICT_TRANSPORT_SECURITY,        "Strict-Transport-Security"        },
{ webc_header_TRAILER,                          "Trailer"                          },
{ webc_header_TRANSFER_ENCODING,                "Transfer-Encoding"                },
{ webc_header_TK,                               "Tk"                               },
{ webc_header_UPGRADE,                          "Upgrade"                          },
{ webc_header_VARY,                             "Vary"                             },
{ webc_header_VIA,                              "Via"                              },
{ webc_header_WARNING,                          "Warning"                          },
{ webc_header_WWW_AUTHENTICATE,                 "WWW-Authenticate"                 },
{ webc_header_X_FRAME_OPTIONS,                  "X-Frame-Options"                  },
{ webc_header_CONTENT_SECURITY_POLICY,          "Content-Security-Policy"          },
{ webc_header_X_CONTENT_SECURITY_POLICY,        "X-Content-Security-Policy"        },
{ webc_header_X_WEBKIT_CSP,                     "X-WebKit-CSP"                     },
{ webc_header_REFRESH,                          "Refresh"                          },
{ webc_header_STATUS,                           "Status"                           },
{ webc_header_TIMING_ALLOW_ORIGIN,              "Timing-Allow-Origin"              },
{ webc_header_X_CONTENT_DURATION,               "X-Content-Duration"               },
{ webc_header_X_CONTENT_TYPE_OPTIONS,           "X-Content-Type-Options"           },
{ webc_header_X_POWERED_BY,                     "X-Powered-By"                     },
{ webc_header_X_REQUEST_ID,                     "X-Request-ID"                     },
{ webc_header_X_CORRELATION_ID,                 "X-Correlation-ID"                 },
{ webc_header_X_UA_COMPATIBLE,                  "X-UA-Compatible"                  },
{ webc_header_X_XSS_PROTECTION,                 "X-XSS-Protection"                 },
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

bool webc_header_set (webc_header_t *header, enum webc_header_name_t name, const char *value)
{
   const char *sname = find_namestring (name);

   if (!header || !value || !sname)
      return false;

   size_t name_len = strlen (sname);
   for (size_t i=0; header->fields && header->fields[i]; i++) {
      if ((strncmp (header->fields[i], sname, name_len))==0) {
         char *tmp = create_header_field (sname, value);
         if (!tmp)
            return false;

         free (header->fields[i]);
         header->fields[i] = tmp;
      }
   }
   return webc_header_add (header, name, value);
}

bool webc_header_add (webc_header_t *header, enum webc_header_name_t name, const char *value)
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

bool webc_header_clear (webc_header_t *header, enum webc_header_name_t name)
{
   const char *sname = find_namestring (name);

   if (!header || !sname)
      return false;

   size_t name_len = strlen (sname);
   for (size_t i=0; header->fields && header->fields[i]; i++) {
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



bool webc_header_write (webc_header_t *header, int fd)
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


const char *headerlist_find (char **headers, enum webc_header_name_t name)
{
   const char *str_name = find_namestring (name);
   if (!str_name)
      return NULL;

   size_t str_len = strlen (str_name);

   for (size_t i=0; headers[i]; i++) {
      if ((strnicmp (headers[i], str_name, str_len))==0) {
         const char *ret = strchr (headers[i], ':');
         if (ret)
            return &ret[1];
         return "";
      }
   }

   return "";
}



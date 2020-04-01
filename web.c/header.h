
#ifndef H_HEADER
#define H_HEADER

#include <stdbool.h>


typedef struct header_t header_t;

enum header_name_t {
   header_ACCESS_CONTROL_ALLOW_ORIGIN,
   header_ACCESS_CONTROL_ALLOW_CREDENTIALS,
   header_ACCESS_CONTROL_EXPOSE_HEADERS,
   header_ACCESS_CONTROL_MAX_AGE,
   header_ACCESS_CONTROL_ALLOW_METHODS,
   header_ACCESS_CONTROL_ALLOW_HEADERS,
   header_ACCEPT_PATCH,
   header_ACCEPT_RANGES,
   header_AGE,
   header_ALLOW,
   header_ALT_SVC,
   header_CACHE_CONTROL,
   header_CONNECTION,
   header_CONTENT_DISPOSITION,
   header_CONTENT_ENCODING,
   header_CONTENT_LANGUAGE,
   header_CONTENT_LENGTH,
   header_CONTENT_LOCATION,
   header_CONTENT_MD5,
   header_CONTENT_RANGE,
   header_CONTENT_TYPE,
   header_DATE,
   header_DELTA_BASE,
   header_ETAG,
   header_EXPIRES,
   header_IM,
   header_LAST_MODIFIED,
   header_LINK,
   header_LOCATION,
   header_P3P,
   header_PRAGMA,
   header_PROXY_AUTHENTICATE,
   header_PUBLIC_KEY_PINS,
   header_RETRY_AFTER,
   header_SERVER,
   header_SET_COOKIE,
   header_STRICT_TRANSPORT_SECURITY,
   header_TRAILER,
   header_TRANSFER_ENCODING,
   header_TK,
   header_UPGRADE,
   header_VARY,
   header_VIA,
   header_WARNING,
   header_WWW_AUTHENTICATE,
   header_X_FRAME_OPTIONS,
   header_CONTENT_SECURITY_POLICY,
   header_X_CONTENT_SECURITY_POLICY,
   header_X_WEBKIT_CSP,
   header_REFRESH,
   header_STATUS,
   header_TIMING_ALLOW_ORIGIN,
   header_X_CONTENT_DURATION,
   header_X_CONTENT_TYPE_OPTIONS,
   header_X_POWERED_BY,
   header_X_REQUEST_ID,
   header_X_CORRELATION_ID,
   header_X_UA_COMPATIBLE,
   header_X_XSS_PROTECTION,
};

#ifdef __cplusplus
extern "C" {
#endif

   header_t *header_new (void);
   void header_del (header_t *header);

   bool header_set (header_t *header, enum header_name_t name, const char *value);
   bool header_add (header_t *header, enum header_name_t name, const char *value);
   bool header_clear (header_t *header, enum header_name_t name);

   bool header_write (header_t *header, int fd);

   const char *headerlist_find (char **headers, enum header_name_t);


#ifdef __cplusplus
};
#endif

#endif



#ifndef H_HEADER
#define H_HEADER

#include <stdbool.h>


typedef struct webc_header_t webc_header_t;

enum webc_header_name_t {
  webc_header_ACCESS_CONTROL_ALLOW_ORIGIN,
  webc_header_ACCESS_CONTROL_ALLOW_CREDENTIALS,
  webc_header_ACCESS_CONTROL_EXPOSE_HEADERS,
  webc_header_ACCESS_CONTROL_MAX_AGE,
  webc_header_ACCESS_CONTROL_ALLOW_METHODS,
  webc_header_ACCESS_CONTROL_ALLOW_HEADERS,
  webc_header_ACCEPT_PATCH,
  webc_header_ACCEPT_RANGES,
  webc_header_AGE,
  webc_header_ALLOW,
  webc_header_ALT_SVC,
  webc_header_CACHE_CONTROL,
  webc_header_CONNECTION,
  webc_header_CONTENT_DISPOSITION,
  webc_header_CONTENT_ENCODING,
  webc_header_CONTENT_LANGUAGE,
  webc_header_CONTENT_LENGTH,
  webc_header_CONTENT_LOCATION,
  webc_header_CONTENT_MD5,
  webc_header_CONTENT_RANGE,
  webc_header_CONTENT_TYPE,
  webc_header_DATE,
  webc_header_DELTA_BASE,
  webc_header_ETAG,
  webc_header_EXPIRES,
  webc_header_IM,
  webc_header_LAST_MODIFIED,
  webc_header_LINK,
  webc_header_LOCATION,
  webc_header_P3P,
  webc_header_PRAGMA,
  webc_header_PROXY_AUTHENTICATE,
  webc_header_PUBLIC_KEY_PINS,
  webc_header_RETRY_AFTER,
  webc_header_SERVER,
  webc_header_SET_COOKIE,
  webc_header_STRICT_TRANSPORT_SECURITY,
  webc_header_TRAILER,
  webc_header_TRANSFER_ENCODING,
  webc_header_TK,
  webc_header_UPGRADE,
  webc_header_VARY,
  webc_header_VIA,
  webc_header_WARNING,
  webc_header_WWW_AUTHENTICATE,
  webc_header_X_FRAME_OPTIONS,
  webc_header_CONTENT_SECURITY_POLICY,
  webc_header_X_CONTENT_SECURITY_POLICY,
  webc_header_X_WEBKIT_CSP,
  webc_header_REFRESH,
  webc_header_STATUS,
  webc_header_TIMING_ALLOW_ORIGIN,
  webc_header_X_CONTENT_DURATION,
  webc_header_X_CONTENT_TYPE_OPTIONS,
  webc_header_X_POWERED_BY,
  webc_header_X_REQUEST_ID,
  webc_header_X_CORRELATION_ID,
  webc_header_X_UA_COMPATIBLE,
  webc_header_X_XSS_PROTECTION,
};

#ifdef __cplusplus
extern "C" {
#endif

   webc_header_t *webc_header_new (void);
   void webc_header_del (webc_header_t *header);

   bool webc_header_set (webc_header_t *header, enum webc_header_name_t name, const char *value);
   bool webc_header_add (webc_header_t *header, enum webc_header_name_t name, const char *value);
   bool webc_header_clear (webc_header_t *header, enum webc_header_name_t name);

   bool webc_header_write (webc_header_t *header, int fd);

   const char *headerlist_find (char **headers, enum webc_header_name_t name);


#ifdef __cplusplus
};
#endif

#endif


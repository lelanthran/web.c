
#ifndef H_UTIL
#define H_UTIL

#define UTIL_LOG(...)      do {\
      fprintf (stderr, "%s:%d: ", __FILE__, __LINE__);\
      fprintf (stderr, __VA_ARGS__);\
} while (0)

#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
};
#endif

#endif



#ifndef H_WEBC_SL
#define H_WEBC_SL

#ifdef __cplusplus
extern "C" {
#endif

   int load_all_cline_opts (int argc, char **argv);
   const char *webc_getenv (const char *name);
   int webc_setenv (const char *name, const char *value);

#ifdef __cplusplus
};
#endif


#endif

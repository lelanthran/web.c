#define _POSIX_C_SOURCE 200809L

#include <string.h>

#include <pthread.h>

#include "webc_resource.h"
#include "webc_handler.h"

/* *************************************************************** */

struct res_rec_t {
   char                       *name;
   char                       *pattern;
   enum webc_pattern_type_t    type;
   webc_resource_handler_t    *handler;
};

static void res_rec_del (struct res_rec_t *rec)
{
   if (rec) {
      free (rec->pattern);
      free (rec->name);
      free (rec);
   }
}

static struct res_rec_t *res_rec_new (const char               *name,
                                      const char               *pattern,
                                      enum webc_pattern_type_t  type,
                                      webc_resource_handler_t  *handler)
{
   struct res_rec_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      return NULL;

   ret->pattern = strdup (pattern);
   ret->name = strdup (name);
   if (!ret->pattern || !ret->name) {
      res_rec_del (ret);
      return NULL;
   }

   ret->type = type;
   ret->handler = handler;

   return ret;
}

/* *************************************************************** */

static struct res_rec_t **g_resources = NULL;
static size_t g_resources_len = 0;

static pthread_mutex_t g_resources_lock;
static bool lock_initialised = false;

static void webc_resource_global_handler_free (void)
{
   webc_resource_global_handler_unlock ();
   pthread_mutex_destroy (&g_resources_lock);
   for (size_t i=0; g_resources[i]; i++) {
      res_rec_del (g_resources[i]);
   }
   free (g_resources);
}

bool webc_resource_global_handler_lock (void)
{
   if (!lock_initialised) {
      pthread_mutexattr_t attr;
      pthread_mutexattr_init (&attr);
      pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
      pthread_mutex_init (&g_resources_lock, &attr);
      pthread_mutexattr_destroy (&attr);
      atexit (webc_resource_global_handler_free);
      lock_initialised = true;
   }
   return pthread_mutex_lock (&g_resources_lock) == 0 ? true : false;
}

bool webc_resource_global_handler_unlock (void)
{
   return pthread_mutex_unlock (&g_resources_lock) == 0 ? true : false;
}

/* *************************************************************** */

bool webc_resource_global_handler_add (const char                *name,
                                       const char                *pattern,
                                       enum webc_pattern_type_t   type,
                                       webc_resource_handler_t   *handler)
{
   struct res_rec_t *rec = res_rec_new (name, pattern, type, handler);
   if (!rec)
      return false;

   struct res_rec_t **tmp = realloc (g_resources,
                                    (g_resources_len + 2) * sizeof *tmp);
   if (!tmp) {
      res_rec_del (rec);
      return false;
   }

   g_resources = tmp;

   g_resources[g_resources_len + 1] = NULL;
   g_resources_len++;

   memmove (&g_resources[1], g_resources,
            (g_resources_len - 1) * sizeof g_resources[0]);

   g_resources[0] = rec;

   return true;
}


webc_resource_handler_t *webc_resource_handler_find (const char *resource)
{
   if (!resource)
      return webc_handler_static_file;

   size_t res_len = strlen (resource);

   for (size_t i=0; g_resources[i]; i++) {
      size_t pattern_len = strlen (g_resources[i]->pattern);
      size_t cmp_len = res_len < pattern_len ? res_len : res_len - pattern_len;

      switch (g_resources[i]->type) {

         case pattern_SUFFIX:
            if ((strncmp (&resource[cmp_len],
                          g_resources[i]->pattern, pattern_len))==0) {
               UTIL_LOG ("Matched suffix: [%s]: [%s]=>[%s]\n",
                          g_resources[i]->pattern,
                          resource,
                          g_resources[i]->name);
               return g_resources[i]->handler;
            }
            break;

         case pattern_PREFIX:
            if (pattern_len > res_len)
               break;
            if ((strncmp (resource, g_resources[i]->pattern, pattern_len))==0) {
               UTIL_LOG ("Matched prefix: [%s]: [%s]=>[%s]\n",
                          g_resources[i]->pattern,
                          resource,
                          g_resources[i]->name);
               return g_resources[i]->handler;
            }
            break;

         case pattern_EXACT:
            if ((strcmp (resource, g_resources[i]->pattern))==0) {
               UTIL_LOG ("Matched exact: [%s]: [%s]=>[%s]\n",
                          g_resources[i]->pattern,
                          resource,
                          g_resources[i]->name);
               return g_resources[i]->handler;
            }
            break;

      }

   }

   UTIL_LOG ("No match for [%s], using handler_static_file()\n", resource);
   return webc_handler_static_file;
}


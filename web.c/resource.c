#include <string.h>

#include <pthread.h>

#include "resource.h"
#include "handler.h"

/* *************************************************************** */

struct res_rec_t {
   char                 *pattern;
   resource_handler_t   *handler;
};

static void res_rec_del (struct res_rec_t *rec)
{
   if (rec) {
      free (rec->pattern);
      free (rec);
   }
}

static struct res_rec_t *res_rec_new (const char *pattern,
                                      resource_handler_t *handler)
{
   struct res_rec_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      return NULL;

   if (!(ret->pattern = strdup (pattern))) {
      free (ret);
      return NULL;
   }

   ret->handler = handler;
   return ret;
}

/* *************************************************************** */

static struct res_rec_t **g_resources = NULL;
static size_t g_resources_len = 0;

static pthread_mutex_t g_resources_lock;
static bool lock_initialised = false;

static void resource_global_handler_free (void)
{
   resource_global_handler_unlock ();
   pthread_mutex_destroy (&g_resources_lock);
   for (size_t i=0; g_resources[i]; i++) {
      res_rec_del (g_resources[i]);
   }
   free (g_resources);
}

bool resource_global_handler_lock (void)
{
   if (!lock_initialised) {
      pthread_mutexattr_t attr;
      pthread_mutexattr_init (&attr);
      pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
      pthread_mutex_init (&g_resources_lock, &attr);
      pthread_mutexattr_destroy (&attr);
      atexit (resource_global_handler_free);
      lock_initialised = true;
   }
   return pthread_mutex_lock (&g_resources_lock) == 0 ? true : false;
}

bool resource_global_handler_unlock (void)
{
   return pthread_mutex_unlock (&g_resources_lock) == 0 ? true : false;
}

/* *************************************************************** */

bool resource_global_handler_add (const char *pattern,
                                  resource_handler_t *handler)
{
   struct res_rec_t *rec = res_rec_new (pattern, handler);
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


resource_handler_t *resource_handler_find (const char *resource)
{
   if (!resource)
      return handler_static_file;

   char *res_ext = strrchr (resource, '.');
   if (!res_ext)
      return handler_static_file;

   size_t res_len = strlen (res_ext);

   for (size_t i=0; g_resources[i]; i++) {
      printf ("Comparing [%s:%s]\n", res_ext, g_resources[i]->pattern);
      if ((strncmp (res_ext, g_resources[i]->pattern, res_len))==0) {
         printf ("Found\n");
         return g_resources[i]->handler;
      }
   }
   return handler_static_file;
}


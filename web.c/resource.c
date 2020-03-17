#include <string.h>

#include <pthread.h>

#include "resource.h"

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

static pthread_mutex_t g_resources_lock;
static bool lock_initialised = false;

static void resource_handler_freelock (void)
{
   resource_handler_unlock ();
   pthread_mutex_destroy (&g_resources_lock);
}

bool resource_handler_lock (void)
{
   if (!lock_initialised) {
      pthread_mutexattr_t attr;
      pthread_mutexattr_init (&attr);
      pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
      pthread_mutex_init (&g_resources_lock, &attr);
      pthread_mutexattr_destroy (&attr);
      atexit (resource_handler_freelock);
      lock_initialised = true;
   }
   return pthread_mutex_lock (&g_resources_lock) == 0 ? true : false;
}

bool resource_handler_unlock (void)
{
   return pthread_mutex_unlock (&g_resources_lock) == 0 ? true : false;
}

/* *************************************************************** */

static struct res_rec_t *g_resources = NULL;
bool resource_handler_add (const char *pattern, resource_handler_t *handler);


resource_handler_t *resource_handler_find (const char *resource)
{
   return NULL;
}


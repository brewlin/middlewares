#ifndef CO_LOCK_H
#define CO_LOCK_H

#include "syncop.h"

typedef enum {
        LOCK_NULL = 0,
        LOCK_TASK,
        LOCK_THREAD
} lock_type_t;

typedef enum {
        SYNC_LOCK_DEFAULT = 0,
        SYNC_LOCK_RECURSIVE,   /*it allows recursive locking*/
} lock_attr_t;


typedef int (*lock_fn_t)   (void *lock);
typedef int (*unlock_fn_t) (void *lock);

struct synclock {
        pthread_mutex_t     guard; /* guard the remaining members, pair @cond */
        pthread_cond_t      cond;  /* waiting non-synctasks */
        struct list_head    waitq; /* waiting synctasks */
        volatile int        lock;  /* true(non zero) or false(zero), lock status */
        lock_attr_t         attr;
        struct synctask    *owner; /* NULL if current owner is not a synctask */
        pthread_t           owner_tid;
        lock_type_t         type;

        //member func
        lock_fn_t           Lock;
        unlock_fn_t         UnLock;
};
typedef struct synclock synclock_t;


synclock_t* lock_create();
void lock_free(synclock_t *lock);
int synclock_init (synclock_t *lock, lock_attr_t attr);
int synclock_destroy (synclock_t *lock);
int __synclock_lock (struct synclock *lock);
int synclock_lock (synclock_t *lock);
int synclock_trylock (synclock_t *lock);
int __synclock_unlock (synclock_t *lock);
int synclock_unlock (synclock_t *lock);
#endif
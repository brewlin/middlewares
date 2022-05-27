#include "syncop.h"
#include "lock.h"

synclock_t* lock_create(){
    synclock_t* lock = malloc(sizeof(synclock_t));

    int ret = synclock_init(lock,SYNC_LOCK_DEFAULT);
    if (ret < 0) {
        printf("synclock_init failed\n");
        return NULL;
    }
    lock->Lock = synclock_lock;
    lock->UnLock = synclock_unlock;
    return lock;
}
void lock_free(synclock_t *lock)
{
    synclock_destroy(lock);
    free(lock);
}

int synclock_init (synclock_t *lock, lock_attr_t attr)
{
        if (!lock)
               return -1;

        pthread_cond_init (&lock->cond, 0);
        lock->type = LOCK_NULL;
        lock->owner = NULL;
        lock->owner_tid = 0;
        lock->lock = 0;
        lock->attr = attr;
        INIT_LIST_HEAD (&lock->waitq);

        return pthread_mutex_init (&lock->guard, 0);
}


int
synclock_destroy (synclock_t *lock)
{
	if (!lock)
		return -1;

	pthread_cond_destroy (&lock->cond);
	return pthread_mutex_destroy (&lock->guard);
}


int
__synclock_lock (struct synclock *lock)
{
        struct synctask *task = NULL;

        if (!lock)
                return -1;

        task = synctask_get ();

        if (lock->lock && (lock->attr == SYNC_LOCK_RECURSIVE)) {
                /*Recursive lock (if same owner requested for lock again then
                 *increment lock count and return success).
                 *Note:same number of unlocks required.
                 */
                switch (lock->type) {
                case LOCK_TASK:
                        if (task == lock->owner) {
                                lock->lock++;
                                printf("Recursive lock called by"
                                              " sync task.owner= %p,lock=%d",
                                              lock->owner, lock->lock);
                                return 0;
                        }
                        break;
                case LOCK_THREAD:
                        if (pthread_equal(pthread_self (), lock->owner_tid)) {
                                lock->lock++;
                                printf("Recursive lock called by"
                                              " thread ,owner=%u lock=%d",
                                              (unsigned int) lock->owner_tid,
                                              lock->lock);
                                return 0;
                        }
                        break;
                default:
                    printf("unknown lock type\n");
                    break;
                }
        }


        while (lock->lock) {
                if (task) {
                        /* called within a synctask */
                        task->woken = 0;
                        list_add_tail (&task->waitq, &lock->waitq);
                        pthread_mutex_unlock (&lock->guard);
                        synctask_yield (task);
                        /* task is removed from waitq in unlock,
                         * under lock->guard.*/
                        pthread_mutex_lock (&lock->guard);
                } else {
                        /* called by a non-synctask */
                        pthread_cond_wait (&lock->cond, &lock->guard);
                }
        }

        if (task) {
                lock->type = LOCK_TASK;
                lock->owner = task;    /* for synctask*/

        } else {
                lock->type = LOCK_THREAD;
                lock->owner_tid = pthread_self (); /* for non-synctask */

        }
        lock->lock = 1;

        return 0;
}


int
synclock_lock (synclock_t *lock)
{
	int ret = 0;

	pthread_mutex_lock (&lock->guard);
	{
		ret = __synclock_lock (lock);
	}
	pthread_mutex_unlock (&lock->guard);

	return ret;
}


int
synclock_trylock (synclock_t *lock)
{
	int ret = 0;

	errno = 0;

	pthread_mutex_lock (&lock->guard);
	{
		if (lock->lock) {
			errno = EBUSY;
			ret = -1;
			goto unlock;
		}

		ret = __synclock_lock (lock);
	}
unlock:
	pthread_mutex_unlock (&lock->guard);

	return ret;
}

//解锁的时候会唤醒，相关等待获取锁的协程
int __synclock_unlock (synclock_t *lock)
{
        struct synctask *task = NULL;
        struct synctask *curr = NULL;

        if (!lock)
               return -1;

        if (lock->lock == 0) {
                printf("Unlock called  before lock \n");
                return -1;
        }
        curr = synctask_get ();
        /*unlock should be called by lock owner
         *i.e this will not allow the lock in nonsync task and unlock
         * in sync task and vice-versa
         */
        switch (lock->type) {
        case LOCK_TASK:
                if (curr == lock->owner) {
                        lock->lock--;
                        // printf("Unlock success %p, remaining locks=%d \n", lock->owner, lock->lock);
                } else {
                        printf("Unlock called by %p, but lock held by %p \n",
                                curr, lock->owner);
                }

                break;
        case LOCK_THREAD:
                if (pthread_equal(pthread_self (), lock->owner_tid)) {
                        lock->lock--;
                        printf("Unlock success %u, remaining "
                                      "locks=%d\n",
                                      (unsigned int)lock->owner_tid,
                                      lock->lock);
                } else {
                        printf("Unlock called by %u, but lock held by %u\n",
                                (unsigned int) pthread_self(),
                                (unsigned int) lock->owner_tid);
                }

                break;
        default:
                break;
        }

        if (lock->lock > 0) {
                 return 0;
        }
        lock->type = LOCK_NULL;
        lock->owner = NULL;
        lock->owner_tid = 0;
        lock->lock = 0;
        pthread_cond_signal (&lock->cond);//也有可能非协程在等待获取锁，所以也需要唤醒对应线程
        if (!list_empty (&lock->waitq)) {
                task = list_entry (lock->waitq.next, struct synctask, waitq);
                list_del_init (&task->waitq);
                synctask_wake (task);
        }

        return 0;
}


//释放锁，会判断这个锁是在协程的场景下加锁还是线程场景下加锁
int synclock_unlock (synclock_t *lock)
{
	int ret = 0;

	pthread_mutex_lock (&lock->guard);
	{
		ret = __synclock_unlock (lock);
	}
	pthread_mutex_unlock (&lock->guard);

	return ret;
}

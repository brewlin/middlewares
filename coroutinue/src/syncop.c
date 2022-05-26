#include "syncop.h"

pthread_key_t synctask_key;

int
synctask_new (struct syncenv *env, synctask_fn_t fn, synctask_cbk_t cbk, void *opaque)
{
        return synctask_new1 (env, 0, fn, cbk, opaque);
}

int
synctask_new1 (struct syncenv *env, size_t stacksize, synctask_fn_t fn,
                synctask_cbk_t cbk, void *opaque)
{
	struct synctask *newtask = NULL;
	int              ret = 0;
        //创建协程任务
	newtask = synctask_create (env, stacksize, fn, cbk, opaque);
	if (!newtask)
		return -1;
        //如果有回调通知,那就需要阻塞等待协程执行完
        if (!cbk)
		ret = synctask_join (newtask);

        return ret;
}

//创建一个协程，并将这个协程投递到全局env->runq上去，会加全局锁
struct synctask *
synctask_create (struct syncenv *env, size_t stacksize, synctask_fn_t fn,
                 synctask_cbk_t cbk, void *opaque)
{
        struct synctask *newtask = NULL;
        int             destroymode = 0;

        VALIDATE_OR_GOTO (fn, err);

        /* Check if the syncenv is in destroymode i.e. destroy is SET.
         * If YES, then don't allow any new synctasks on it. Return NULL.
         */
        pthread_mutex_lock (&env->mutex);
        {
                destroymode = env->destroy; //如果协程调度器进入了 销毁模式，则不能再创建协程
        }
        pthread_mutex_unlock (&env->mutex);

        /* syncenv is in DESTROY mode, return from here */
        if (destroymode)
                return NULL;
        //创建任务
        newtask = malloc(sizeof(*newtask));
        if (!newtask)
                return NULL;

        newtask->env        = env;
        newtask->syncfn     = fn; //真正要执行的方法
        newtask->synccbk    = cbk; //异步回调通知
        newtask->opaque     = opaque; //函数参数


        INIT_LIST_HEAD (&newtask->all_tasks);
        INIT_LIST_HEAD (&newtask->waitq);
        //将当前上下文保存到task中
        if (getcontext (&newtask->ctx) < 0) {
            printf("syncop getcontext failed\n");
            goto err;
        }
        //默认申请2M的栈大小
        if (stacksize <= 0) {
            newtask->stack = malloc(env->stacksize);
            newtask->ctx.uc_stack.ss_size = env->stacksize;
        } else {
            newtask->stack = malloc(stacksize);
            newtask->ctx.uc_stack.ss_size = stacksize;
        }

        if (!newtask->stack) {
                goto err;
        }

        newtask->ctx.uc_stack.ss_sp   = newtask->stack;
        //设置协程需要执行的方法,统一用synctask_wrap进行包装
        //makecontext不设置 下文
        makecontext (&newtask->ctx, (void (*)(void)) synctask_wrap, 0);
        //标记为初始化
        newtask->state = SYNCTASK_INIT;
        //标记为睡眠状态
        newtask->slept = 1;

        if (!cbk) {
                pthread_mutex_init (&newtask->mutex, NULL);
                pthread_cond_init (&newtask->cond, NULL);
                newtask->done = 0;
        }
        //投递到全局runq队列中去
        synctask_wake (newtask);
        /*
         * Make sure someone's there to execute anything we just put on the
         * run queue.
         */
        //判断下协程调度器线程不够用了需要扩缩容
        syncenv_scale(env);

	return newtask;
err:
        if (newtask) {
                free (newtask->stack);
                free (newtask);
        }

        return NULL;
}

void synctask_wrap (void)
{
        struct synctask *task = NULL;

        /* Do not trust the pointer received. It may be
           wrong and can lead to crashes. */

        task = synctask_get ();
        task->ret = task->syncfn (task->opaque);
        if (task->synccbk)
                task->synccbk (task->ret, task->opaque);

        task->state = SYNCTASK_DONE;
        //切换会调度器中继续运行
        synctask_yield (task);
}

void
synctask_wake (struct synctask *task)
{
        //协程唤醒，从新将协程加入到 全局runq队列中去
        struct syncenv *env = NULL;

        env = task->env;

        pthread_mutex_lock (&env->mutex);
        {
                task->woken = 1;
                //当前协程处于睡眠状态的时候才会去唤醒，否则可能唤醒一个正在运行的协程，状态出现异常
                if (task->slept)
                        __run (task);

		pthread_cond_broadcast (&env->cond);
        }
        pthread_mutex_unlock (&env->mutex);
}


void
synctask_yield (struct synctask *task)
{
#if defined(__NetBSD__) && defined(_UC_TLSBASE)
        /* Preserve pthread private pointer through swapcontex() */
        task->proc->sched.uc_flags &= ~_UC_TLSBASE;
#endif

        if (task->state != SYNCTASK_DONE) {
                task->state = SYNCTASK_SUSPEND;
                // (void) gf_backtrace_save (task->btbuf);
        }
        //将当前context保存到 task->ctx,然后跳转到task->proc->sched上去执行
        //task->proc->sched 就是线程调度器，会恢复到线程调度的那个循环里面
        if (swapcontext (&task->ctx, &task->proc->sched) < 0) {
                printf("syncop: swapcontext failed\n");
        }
}
void
synctask_destroy (struct synctask *task)
{
        if (!task)
                return;

        free (task->stack);


        if (task->synccbk == NULL) {
               pthread_mutex_destroy (&task->mutex);
               pthread_cond_destroy (&task->cond);
        }

        free (task);
}

void
synctask_done (struct synctask *task)
{
        if (task->synccbk) {
                synctask_destroy (task);
                return;
        }

        pthread_mutex_lock (&task->mutex);
        {
		task->state = SYNCTASK_ZOMBIE;
                task->done = 1;
                pthread_cond_broadcast (&task->cond);
        }
        pthread_mutex_unlock (&task->mutex);
}

int
synctask_join (struct synctask *task)
{
	int ret = 0;

	pthread_mutex_lock (&task->mutex);
	{
		while (!task->done)
			pthread_cond_wait (&task->cond, &task->mutex);
	}
	pthread_mutex_unlock (&task->mutex);

	ret = task->ret;

	synctask_destroy (task);

	return ret;
}
void syncenv_destroy (struct syncenv *env)
{
        if (env == NULL)
                return;

        pthread_mutex_lock (&env->mutex);
        {
                env->destroy = 1;
                /* This broadcast will wake threads in pthread_cond_wait
                 * in syncenv_task
                 */
                pthread_cond_broadcast (&env->cond);

                /* when the syncenv_task() thread is exiting, it broadcasts to
                 * wake the below wait.
                 */
                while (env->procs != 0) {
                        pthread_cond_wait (&env->cond, &env->mutex);
                }
        }
        pthread_mutex_unlock (&env->mutex);

        pthread_mutex_destroy (&env->mutex);
        pthread_cond_destroy (&env->cond);

        free (env);

        return;
}
int synctask_set (void *synctask)
{
        int     ret = 0;

        pthread_setspecific (synctask_key, synctask);

        return ret;
}


void *synctask_get ()
{
        void   *synctask = NULL;
        //获取线程缓存
        synctask = pthread_getspecific (synctask_key);

        return synctask;
}

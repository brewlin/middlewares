#include "syncop.h"

//协程调度器
void * syncenv_processor (void *thdata)
{
        struct syncenv  *env = NULL;
        struct syncproc *proc = NULL;
        struct synctask *task = NULL;

        proc = thdata;
        env = proc->env;
        printf("\n 协程调度器 started\n");

        for (;;) {
                //获取一个协程任务
                task = syncenv_task (proc);
                //1. 不需要释放当前线程的话，在里面会通过cond进行阻塞当前线程
                //2. 需要释放的话，直接返回NULL，当前直接结束线程
                if (!task) 
                        break;

                synctask_switchto (task);
                //判断是否需要调度器扩缩容
                syncenv_scale (env);
        }

        return NULL;
}

struct syncenv *
syncenv_new (size_t stacksize, int procmin, int procmax)
{
        struct syncenv *newenv = NULL;
        int             ret = 0;
        int             i = 0;
        char            thread_name[GF_THREAD_NAMEMAX] = {0,};

	if (!procmin || procmin < 0)
		procmin = SYNCENV_PROC_MIN;
	if (!procmax || procmax > SYNCENV_PROC_MAX)
		procmax = SYNCENV_PROC_MAX;

	if (procmin > procmax)
		return NULL;

        newenv = malloc(sizeof(*newenv));

        if (!newenv)
                return NULL;

        pthread_mutex_init (&newenv->mutex, NULL);
        pthread_cond_init (&newenv->cond, NULL);

        INIT_LIST_HEAD (&newenv->runq);
        INIT_LIST_HEAD (&newenv->waitq);

        newenv->stacksize    = SYNCENV_DEFAULT_STACKSIZE;
        if (stacksize)
                newenv->stacksize = stacksize;
        newenv->procmin = procmin;
        newenv->procmax = procmax;

        //默认创建2个，最大会动态扩容至16个线程
        for (i = 0; i < newenv->procmin; i++) {
                newenv->proc[i].env = newenv;
                snprintf (thread_name, sizeof(thread_name),
                          "%s%d", "sproc", (newenv->procs));
                ret = gf_thread_create (&newenv->proc[i].processor, NULL,
                                        syncenv_processor, &newenv->proc[i],
                                        thread_name);
                if (ret)
                        break;
                //保存已经创建的线程数
                newenv->procs++;
        }

        if (ret != 0) {
                syncenv_destroy (newenv);
                newenv = NULL;
        }

        return newenv;
}



static void
__run (struct synctask *task)
{
        struct syncenv *env = NULL;

        env = task->env;

        list_del_init (&task->all_tasks);
        switch (task->state) {
        case SYNCTASK_INIT:
        case SYNCTASK_SUSPEND:
                break;
        case SYNCTASK_RUN://协程运行
                printf("re-runing already runing task\n");
                env->runcount--;
                break;
        case SYNCTASK_WAIT://协程等待
                env->waitcount--;
                break;
        case SYNCTASK_DONE://协程已完结
                printf("runing completed task\n");
		        return;
	    case SYNCTASK_ZOMBIE://僵尸协程
                printf("attempted to wake up zombie!!\n");
		        return;
        }
        //投递到协程队列中，等待执行
        list_add_tail (&task->all_tasks, &env->runq);
        //协程总数 + 1
        env->runcount++;
        //标记为可运行
        task->state = SYNCTASK_RUN;
}
void
synctask_switchto (struct synctask *task)
{
        struct syncenv *env = NULL;

        env = task->env;
        //将task保存到当前线程变量上
        synctask_set (task);

#if defined(__NetBSD__) && defined(_UC_TLSBASE)
        /* Preserve pthread private pointer through swapcontex() */
        task->ctx.uc_flags &= ~_UC_TLSBASE;
#endif
        //保存当前线程调度器上下文到sched上,然后在跳转到task协程任务上
        if (swapcontext (&task->proc->sched, &task->ctx) < 0) {
            printf("syncop swapcontext failed\n");
        }
        //该协程运行完， 
        if (task->state == SYNCTASK_DONE) {
                synctask_done (task);
                return;
        }

        pthread_mutex_lock (&env->mutex);
        {
                if (task->woken) {
                        __run (task);
                } else { //需要睡眠则将该协程保存到队列里，等待唤醒
                        task->slept = 1;
                        __wait (task);
                }
        }
        pthread_mutex_unlock (&env->mutex);
}

void
syncenv_scale (struct syncenv *env)
{
        int  diff = 0;
        int  scale = 0;
        int  i = 0;
        int  ret = 0;
        char thread_name[GF_THREAD_NAMEMAX] = {0,};

        pthread_mutex_lock (&env->mutex);
        {
                
                if (env->procs > env->runcount)
                        goto unlock;

                scale = env->runcount;
                if (scale > env->procmax)
                        scale = env->procmax;
                if (scale > env->procs)
                        diff = scale - env->procs;
                while (diff) {
                        diff--;
                        for (; (i < env->procmax); i++) {
                                if (env->proc[i].processor == 0)
                                        break;
                        }

                        env->proc[i].env = env;
                        snprintf (thread_name, sizeof(thread_name),
                                  "%s%d", "sproc", env->procs);
                        ret = gf_thread_create (&env->proc[i].processor, NULL,
                                                syncenv_processor,
                                                &env->proc[i], thread_name);
                        if (ret)
                                break;
                        env->procs++;
                        i++;
                }
        }
unlock:
        pthread_mutex_unlock (&env->mutex);
}

static void
__wait (struct synctask *task)
{
        struct syncenv *env = NULL;

        env = task->env;

        list_del_init (&task->all_tasks);
        switch (task->state) {
        case SYNCTASK_INIT:
        case SYNCTASK_SUSPEND:
                break;
        case SYNCTASK_RUN:
                env->runcount--;
                break;
        case SYNCTASK_WAIT:
                printf("re-waiting already waiting\n");
                env->waitcount--;
                break;
        case SYNCTASK_DONE:
                printf("runing completed task\n");
                return;
	case SYNCTASK_ZOMBIE:
                printf("attempted to sleep a zombie!!\n");
		return;
        }

        list_add_tail (&task->all_tasks, &env->waitq);
        env->waitcount++;
        task->state = SYNCTASK_WAIT;
}

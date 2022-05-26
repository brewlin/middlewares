#ifndef GLUSTERFS_COROUTINE_H
#define GLUSTERFS_COROUTINE_H

#include <sys/time.h>
#include <pthread.h>
#include <ucontext.h>
#include <stdlib.h>
#include "list.h"
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>

#define GF_BACKTRACE_LEN        4096
#define SYNCENV_PROC_MAX 16
#define SYNCENV_PROC_MIN 2
#define SYNCPROC_IDLE_TIME 600
#define SYNCENV_DEFAULT_STACKSIZE (2 * 1024 * 1024)
/* pthread related */
#define GF_THREAD_NAMEMAX 9
#define GF_THREAD_NAME_PREFIX "coroutine"
#define GF_THREAD_NAME_PREFIX_LEN 7

typedef int (*synctask_cbk_t) (int ret, void *opaque);

typedef int (*synctask_fn_t) (void *opaque);

typedef enum {
        SYNCTASK_INIT = 0,
        SYNCTASK_RUN,
        SYNCTASK_SUSPEND,
        SYNCTASK_WAIT,
        SYNCTASK_DONE,
	SYNCTASK_ZOMBIE,
} synctask_state_t;

/* for one sequential execution of @syncfn */
struct synctask {
        struct list_head    all_tasks;
        struct syncenv     *env;
        synctask_cbk_t      synccbk;
        synctask_fn_t       syncfn;
        synctask_state_t    state;
        void               *opaque;
        void               *stack;
        int                 woken;
        int                 slept;
        int                 ret;


        ucontext_t          ctx;
        struct syncproc    *proc;

        pthread_mutex_t     mutex; /* for synchronous spawning of synctask */
        pthread_cond_t      cond;
        int                 done;

	struct list_head    waitq; /* can wait only "once" at a time */
        char                btbuf[GF_BACKTRACE_LEN];
};


struct syncproc {
        pthread_t           processor;
        ucontext_t          sched;
        struct syncenv     *env;
        struct synctask    *current;
};
/* hosts the scheduler thread and framework for executing synctasks */
struct syncenv {
        struct syncproc     proc[SYNCENV_PROC_MAX];
        int                 procs;

        struct list_head    runq;
        int                 runcount;
        struct list_head    waitq;
        int                 waitcount;

	int                 procmin;
	int                 procmax;

        pthread_mutex_t     mutex;
        pthread_cond_t      cond;

        size_t              stacksize;

        int                 destroy; /* FLAG to mark syncenv is in destroy mode
                                        so that no more synctasks are accepted*/
};

#define VALIDATE_OR_GOTO(arg,label)   do {				\
		if (!arg) {						\
			printf(" invalid argument:\n");                 \
			goto label;					\
		}							\
	} while (0)



//----------------------------------函数声明-------------------------------
int synctask_new (struct syncenv *env, synctask_fn_t fn, synctask_cbk_t cbk, void *opaque);
int synctask_new1 (struct syncenv *env, size_t stacksize, synctask_fn_t fn,
                synctask_cbk_t cbk, void *opaque);
struct synctask * synctask_create (struct syncenv *env, size_t stacksize, synctask_fn_t fn,
                 synctask_cbk_t cbk, void *opaque);
struct syncenv * syncenv_new (size_t stacksize, int procmin, int procmax);
struct synctask * syncenv_task (struct syncproc *proc);

void synctask_wrap (void);
void synctask_wake (struct synctask *task);
void synctask_yield (struct synctask *task);
void synctask_destroy (struct synctask *task);
void synctask_done (struct synctask *task);
int  synctask_join (struct synctask *task);


int gf_thread_create (pthread_t *thread, const pthread_attr_t *attr,
                  void *(*start_routine)(void *), void *arg, const char *name);
void * syncenv_processor (void *thdata);
void __run (struct synctask *task);
void synctask_switchto (struct synctask *task);
void syncenv_scale (struct syncenv *env);
void __wait (struct synctask *task);
void syncenv_destroy (struct syncenv *env);
int synctask_set (void *synctask);
#endif
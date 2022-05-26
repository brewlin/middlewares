#include "syncop.h"
int
gf_thread_create (pthread_t *thread, const pthread_attr_t *attr,
                  void *(*start_routine)(void *), void *arg, const char *name)
{
        sigset_t set, old;
        int ret;
        char thread_name[GF_THREAD_NAMEMAX+GF_THREAD_NAME_PREFIX_LEN] = {0,};
        /* Max name on Linux is 16 and on NetBSD is 32
         * All Gluster threads have a set prefix of gluster and hence the limit
         * of 9 on GF_THREAD_NAMEMAX including the null character.
         */

        sigemptyset (&old);
        sigfillset (&set);
        sigdelset (&set, SIGSEGV);
        sigdelset (&set, SIGBUS);
        sigdelset (&set, SIGILL);
        sigdelset (&set, SIGSYS);
        sigdelset (&set, SIGFPE);
        sigdelset (&set, SIGABRT);

        pthread_sigmask (SIG_BLOCK, &set, &old);

        ret = pthread_create (thread, attr, start_routine, arg);
        snprintf (thread_name, sizeof(thread_name), "%s%s",
                  GF_THREAD_NAME_PREFIX, name);

        if (0 == ret && name) {
                #ifdef GF_LINUX_HOST_OS
                        pthread_setname_np(*thread, thread_name);
                #elif defined(__NetBSD__)
                        pthread_setname_np(*thread, thread_name, NULL);
                #elif defined(__FreeBSD__)
                        pthread_set_name_np(*thread, thread_name);
                #else
                        printf("could not set thread name:%s\n",thread_name);
                #endif
        }

        pthread_sigmask (SIG_SETMASK, &old, NULL);

        return ret;
}

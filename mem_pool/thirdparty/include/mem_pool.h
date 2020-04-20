
#ifndef _PALLOC_H_INCLUDED_
#define _PALLOC_H_INCLUDED_


#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef intptr_t        int_t;
typedef uintptr_t       uint_t;

#define  OK          0
#define  ERROR       -1

#define memzero(buf, n)       (void) memset(buf, 0, n)
#define align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))


extern uint_t  pagesize;
#define MAX_ALLOC_FROM_POOL  (pagesize - 1)
#define DEFAULT_POOL_SIZE    (16 * 1024)
#define POOL_ALIGNMENT       16
#define ALIGNMENT   sizeof(unsigned long)    /* platform word */

typedef void (*pool_cleanup_pt)(void *data);
typedef struct pool_cleanup_s  pool_cleanup_t;
struct pool_cleanup_s {
    pool_cleanup_pt   handler;
    void                 *data;
    pool_cleanup_t   *next;
};

typedef struct pool_large_s  pool_large_t;
struct pool_large_s {
    pool_large_t     *next;
    void             *alloc;
};

typedef struct pool_s        pool_t;
typedef struct {
    u_char               *last;
    u_char               *end;
    pool_t               *next;
    uint_t               failed;
} pool_data_t;

struct pool_s {
    pool_data_t      d;
    size_t           max;
    pool_t           *current;
    pool_large_t     *large;
    pool_cleanup_t   *cleanup;
};


pool_t *create_pool(size_t size);
void destroy_pool(pool_t *pool);
void reset_pool(pool_t *pool);

void *palloc(pool_t *pool, size_t size);
void *pnalloc(pool_t *pool, size_t size);
void *pcalloc(pool_t *pool, size_t size);
void *pmemalign(pool_t *pool, size_t size, size_t alignment);
int_t pfree(pool_t *pool, void *p);
pool_cleanup_t *pool_cleanup_add(pool_t *p, size_t size);

#if (HAS_POSIX_MEMALIGN || HAS_MEMALIGN)
void * mem_memalign(size_t aligment,size_t size);
#else
#define mem_memalign(aligment,size) malloc(size)
#endif


#endif /* _PALLOC_H_INCLUDED_ */

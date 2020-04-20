#ifndef _ARRAY_H_INCLUDED_
#define _ARRAY_H_INCLUDED_

#include "config.h"
#include "mem_pool.h"


struct array_s {
    void        *elts;
    uint_t   nelts;
    size_t       size;
    uint_t   nalloc;
    pool_t  *pool;
};


array_t *array_create(pool_t *p, uint_t n, size_t size);
void array_destroy(array_t *a);
void *array_push(array_t *a);
void *array_push_n(array_t *a, uint_t n);


static inline int_t
array_init(array_t *array, pool_t *pool, uint_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    array->elts = palloc(pool, n * size);
    if (array->elts == NULL) {
        return ERROR;
    }

    return OK;
}


#endif /* _ARRAY_H_INCLUDED_ */

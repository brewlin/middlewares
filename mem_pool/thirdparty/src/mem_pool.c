#include "mem_pool.h"

uint_t  pagesize;

static void *palloc_small(pool_t *pool, size_t size,uint_t align);
static void *palloc_block(pool_t *pool, size_t size);
static void *palloc_large(pool_t *pool, size_t size);

//创建内存池，默认分配的内存包含了pool_t结构体的大小，所以实际可分配内存为size - sizeof(pool_t);
pool_t *create_pool(size_t size)
{
    pool_t  *p;
    //分配对齐内存
    p = mem_memalign(POOL_ALIGNMENT, size);
    if (p == NULL) {
        return NULL;
    }

    p->d.last = (u_char *) p + sizeof(pool_t);
    p->d.end = (u_char *) p + size;
    p->d.next = NULL;
    p->d.failed = 0;

    size = size - sizeof(pool_t);
    p->max = (size < MAX_ALLOC_FROM_POOL) ? size : MAX_ALLOC_FROM_POOL;

    p->current = p;
    p->large = NULL;
    p->cleanup = NULL;

    return p;
}

//内存池销毁
//1.调用所有注册在pool_t上的清理事件
//2.清理large内存块
//3.清理所有pool_t内存块
void destroy_pool(pool_t *pool)
{
    pool_t          *p, *n;
    pool_large_t    *l;
    pool_cleanup_t  *c;

    for (c = pool->cleanup; c; c = c->next) {
        if (c->handler) {
            c->handler(c->data);
        }
    }

    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            free(l->alloc);
        }
    }

    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
        free(p);

        if (n == NULL) {
            break;
        }
    }
}

//重置内存池
//1.销毁所有的large内存块
//2.复位每个pool_t内存块的last起始位置，以前的数据不再生效
void reset_pool(pool_t *pool)
{
    pool_t        *p;
    pool_large_t  *l;

    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            free(l->alloc);
        }
    }

    for (p = pool; p; p = p->d.next) {
        p->d.last = (u_char *) p + sizeof(pool_t);
        p->d.failed = 0;
    }

    pool->current = pool;
    pool->large = NULL;
}

//分配地址对齐的内存
void *palloc(pool_t *pool, size_t size)
{
    if (size <= pool->max) {
        return palloc_small(pool, size, 1);
    }
    return palloc_large(pool, size);
}

//分配内存时不对齐内存
void * pnalloc(pool_t *pool, size_t size)
{
    if (size <= pool->max) {
        return palloc_small(pool, size, 0);
    }

    return palloc_large(pool, size);
}

//分配内存主函数
static inline void *palloc_small(pool_t *pool, size_t size, uint_t align)
{
    u_char      *m;
    pool_t  *p;

    p = pool->current;

    do {
        m = p->d.last;

        if (align) {
            m = align_ptr(m, ALIGNMENT);
        }

        if ((size_t) (p->d.end - m) >= size) {
            p->d.last = m + size;

            return m;
        }

        p = p->d.next;

    } while (p);

    return palloc_block(pool, size);
}


static void *palloc_block(pool_t *pool, size_t size)
{
    u_char      *m;
    size_t       psize;
    pool_t  *p, *new;

    psize = (size_t) (pool->d.end - (u_char *) pool);

    m = mem_memalign(POOL_ALIGNMENT, psize);
    if (m == NULL) {
        return NULL;
    }

    new = (pool_t *) m;

    new->d.end = m + psize;
    new->d.next = NULL;
    new->d.failed = 0;

    m += sizeof(pool_data_t);
    m = align_ptr(m, ALIGNMENT);
    new->d.last = m + size;

    for (p = pool->current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            pool->current = p->d.next;
        }
    }

    p->d.next = new;

    return m;
}

//分配大块内存主函数
static void *palloc_large(pool_t *pool, size_t size)
{
    void              *p;
    uint_t         n;
    pool_large_t  *large;

    p = malloc(size);
    if (p == NULL) {
        return NULL;
    }

    n = 0;

    for (large = pool->large; large; large = large->next) {
        if (large->alloc == NULL) {
            large->alloc = p;
            return p;
        }

        if (n++ > 3) {
            break;
        }
    }

    large = palloc_small(pool, sizeof(pool_large_t), 1);
    if (large == NULL) {
        free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}

//分配对其内存，并挂到large链表上
void *pmemalign(pool_t *pool, size_t size, size_t alignment)
{
    void              *p;
    pool_large_t  *large;

    p = mem_memalign(alignment, size);
    if (p == NULL) {
        return NULL;
    }

    large = palloc_small(pool, sizeof(pool_large_t), 1);
    if (large == NULL) {
        free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}

//回收指定large内存
int_t pfree(pool_t *pool, void *p)
{
    pool_large_t  *l;

    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
            free(l->alloc);
            l->alloc = NULL;

            return OK;
        }
    }

    return ERROR;
}

//分配对其并初始化该段内存
void *pcalloc(pool_t *pool, size_t size)
{
    void *p;

    p = palloc(pool, size);
    if (p) {
        memzero(p, size);
    }

    return p;
}

//注册清除资源事件，内存回收时会调用该回调函数清除相关自定义资源
pool_cleanup_t *pool_cleanup_add(pool_t *p, size_t size)
{
    pool_cleanup_t  *c;

    c = palloc(p, sizeof(pool_cleanup_t));
    if (c == NULL) {
        return NULL;
    }

    if (size) {
        c->data = palloc(p, size);
        if (c->data == NULL) {
            return NULL;
        }

    } else {
        c->data = NULL;
    }

    c->handler = NULL;
    c->next = p->cleanup;

    p->cleanup = c;

    return c;
}


#if (HAS_POSIX_MEMALIGN)
void *mem_memalign(size_t alignment, size_t size)
{
    void  *p;
    int err = posix_memalign(&p, alignment, size);
    if (err) {
        p = NULL;
    }
    return p;
}
#elif (HAS_MEMALIGN)
void *mem_memalign(size_t alignment, size_t size)
{
    return memalign(alignment, size);
}
#endif
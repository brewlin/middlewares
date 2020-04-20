#include "mem_pool.h"
typedef struct {
    int              fd;
    u_char           *name;
} file_t;

//默认的文件清除事件
void clean(void *data)
{
    file_t  *c = data;
    printf("%d ",c->fd);
    // if (close(c->fd) == ERROR) {
    // }
}

//删除指定文件
void delete(void *data)
{
    file_t  *c = data;
    int  err;
    if (unlink((const char *)c->name) == ERROR) {
        err = errno;
        if (err != 2) {
        }
    }
    if (close(c->fd) == ERROR) {
    }
}

int main(){
    pagesize = getpagesize();
    
    pool_t *pool;
    pool = create_pool(DEFAULT_POOL_SIZE);
    for(int i = 0; i < 100; i ++){
        pnalloc(pool,sizeof(file_t));
    }
    for(int i = 0; i < 100; i ++){
        pool_cleanup_t *c = pool_cleanup_add(pool,sizeof(file_t));
        ((file_t *)c->data)->fd = i; 
        c->handler  = clean; 
    }
    destroy_pool(pool);
    return 0;
}
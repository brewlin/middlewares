#include "syncop.h"
#include "lock.h"

mutex *lock;//创建全局锁

int count = 50;
int co(void *data){
    task* t = synctask_get ();
    printf("co%d started %ld\n",(int)data,t->proc->processor);
    lock->Lock(lock); //没有拿到锁会暂停当前协程，切换出去
    return 0;
}
int co_callback(int ret ,void *data){
    printf("co%d end \n",(int)data);
    count --;
    lock->UnLock(lock); //恢复还在拿锁的对应协程
}
int co2(void *data){
    while(1){
        lock->Lock(lock);
        if(count < 1) { //所有任务已经完成
            lock->UnLock(lock);
            break;
        }
        lock->UnLock(lock);
    }
}
void main(){
    //全局调度器
    scheduler *sched = syncenv_new(0,10,0);
    sleep(5);//等待调度线程完全创建
    lock = lock_create();//创建一个锁

    for(int i = 0 ; i < 50 ; i ++){
        //创建一个协程,无需阻塞等待结果
        go(sched,co,co_callback,(void*)i);
    }
    //阻塞等待所有协程任务完成
    go(sched,co2,NULL,NULL);
    lock->Free(lock);
}

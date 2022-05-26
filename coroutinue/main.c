#include "syncop.h"
typedef struct syncenv sched;
typedef struct synctask task;

task *t1 = NULL;
int done = 0;
int co1(void *data){
    printf("%s started\n",(char*)data);

    //切出当前协程
    task *curr = synctask_get();
    t1 = curr;
    if(curr)   synctask_yield(curr);

    printf("recoverd right now!\n");
    return 0;
}
int co2(void *data){
    printf("%s started\n",(char*)data);
}
int co1_callback(int ret ,void *data){
    printf("co %s end\n",(char*)data);
    done=1;
}
int co2_callback(int ret ,void *data){
    printf("co %s end\n",(char*)data);
    //可能co1还没执行
    while(t1==NULL){sleep(1);}
    //唤醒co1
    synctask_wake(t1);
}
int co3(void *data){
    printf("%s started\n",(char*)data);
    while(done == 0){
        sleep(1);
    }
}
void main(){

    //全局调度器
    sched *go = syncenv_new(0,0,0);
    //创建一个协程1,无需阻塞等待结果
    synctask_new(go,co1,co1_callback,"co1");
    //创建一个协程2,无需阻塞等待结果
    synctask_new(go,co2,co2_callback,"co2");

    //创建协程3，阻塞等待结果
    synctask_new(go,co3,NULL,"co3");
}
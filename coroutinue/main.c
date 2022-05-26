#include "syncop.h"
typedef struct syncenv sched;

int co1(void *data){
    printf("%s\n",(char*)data);
} 
int co2(void *data){
    printf("%s\n",(char*)data);
}

void main(){

    //全局调度器
    sched *go = syncenv_new(0,0,0);
    //创建一个协程
    synctask_new(go,co1,NULL,"co1");
    //创建一个协程
    synctask_new(go,co2,NULL,"co2");
    sleep(100);
}
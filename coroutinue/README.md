```
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
```
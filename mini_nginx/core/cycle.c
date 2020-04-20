#include "cycle.h"
#include "mem_pool.h"
#include "array.h"
#include "connection.h"
#include "config.h"
#include "log.h"
#include "event.h"

cycle_t *GCYCLE;
module_t* modules[] = {
    &http_core_module,
    &event_core_module,
};

// 以守护进程的方式启动
static void daemonize(cycle_t *cycle) {
    
    if(!cycle->is_daemon)
        return;

    int null_fd = open("/dev/null", O_RDWR);
    struct sigaction sa;
    int fd0, fd1, fd2;

    umask(0);

    // fork()新的进程
    switch(fork()) {
        case 0:
            break;
        case -1:
            log_error(cycle->log, "daemon fork 1: %s", strerror(errno));
            exit(1);
        default:
            exit(0);
    }
    setsid();

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGHUP,  &sa, NULL) < 0) {
        log_error(cycle->log, "SIGHUP: %s", strerror(errno));
        exit(1);
    }

    switch(fork()) {
        case 0:
            break;
        case -1:
            log_error(cycle->log, "daemon fork 2: %s", strerror(errno));
            exit(1);
        default:
            exit(0);
    }

    chdir("/");

    // 关闭标准输入，输出，错误
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    fd0 = dup(null_fd);
    fd1 = dup(null_fd);
    fd2 = dup(null_fd);

    if (null_fd != -1)
        close(null_fd);

    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        log_error(cycle->log, "unexpected fds: %d %d %d", fd0, fd1, fd2);
        exit(1);
    }

    log_info(cycle->log, "pid: %d", getpid());
} 


//创建初始化cycle 全局结构体
static cycle_t * init()
{
    pagesize = getpagesize();
    cycle_t * cycle = (cycle_t*) malloc(sizeof(cycle_t));
    memzero(cycle,sizeof(cycle_t));
    cycle->pool = create_pool(1024);
    cycle->pool->max = MAX_ALLOC_FROM_POOL;
    GCYCLE = cycle;//global cycle

    //初始化监听端口链表
    if (array_init(&cycle->listening, cycle->pool, 10,sizeof(listening_t)) != OK){
        return NULL;
    }

    // 打开日志文件
    cycle->log = palloc(cycle->pool,sizeof(log_t));
    cycle->log->use_logfile = 1;
    log_open(cycle->log, "./run.log");

    //开启守护进程
    cycle->is_daemon = 0;
    daemonize(cycle);

    return cycle; 

}
//模拟nginx模块注册流程
static void init_module(cycle_t *cycle)
{
    log_info(cycle->log,"cycle: init module");
    cycle->modules_n = 2;
    cycle->modules = modules;
}
//启动模块
int_t start_module(cycle_t *cycle){
    log_info(cycle->log,"cycle: start module");
    for(int i = 0;i < cycle->modules_n ; i++){
        //http 注册tcp监听端口
        //event 模块创建epoll  epoll_events
        if(cycle->modules[i]->init_process(cycle) != OK){
            log_error(cycle->log,"cycle: start module init process error");
            return ERROR;
        }
    }
    return OK;
}

cycle_t* init_cycle(){
    //初始化全局结构体
    cycle_t* cycle = init();

    //初始化模块
    init_module(cycle);

    //启动模块
    if(start_module(cycle) != OK){
        return cycle;
    }

    //epoll wait 分发事件
    while(1){
        if(process_events(cycle,event_flags) == ERROR){
            goto end;
        }
    }
end:
    close_listening_sockets(cycle);
    
    return cycle;
}
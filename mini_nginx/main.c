#include "cycle.h"
#include "mem_pool.h"

int main()
{
    cycle_t * cycle = init_cycle();
    //程序结束 回收内存池内存    
    if(cycle->pool){
        destroy_pool(cycle->pool);
    }
    if(cycle->connections){
        free(cycle->connections);
    }
    if(cycle->read_events){
        free(cycle->read_events);
    }
    if(cycle->write_events){
        free(cycle->write_events);
    }
    free(cycle);
}
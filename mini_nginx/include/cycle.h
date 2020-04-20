#ifndef _CYCLE_H_INCLUDED_
#define _CYCLE_H_INCLUDED_

#include "config.h"
#include "array.h"

#define string(str) {sizeof(str) - 1,(u_char*)str}



struct cycle_s {
    void                  ****conf_ctx;
    pool_t               *pool;


    connection_t         *free_connections;
    uint_t                free_connection_n;

    module_t             **modules;
    uint_t               modules_n;
    array_t               listening;

    uint_t                connection_n;

    connection_t         *connections;
    event_t              *read_events;
    event_t              *write_events;

    int_t                is_daemon;
    log_t                *log;

};


struct module_s{
    int_t           (*init_master)();

    int_t           (*init_module)(cycle_t *cycle);

    int_t           (*init_process)(cycle_t *cycle);
    int_t           (*init_thread)(cycle_t *cycle);
    void                (*exit_thread)(cycle_t *cycle);
    void                (*exit_process)(cycle_t *cycle);

    void                (*exit_master)(cycle_t *cycle);
};

extern module_t event_core_module;
extern module_t http_core_module;
extern module_t* modules[];

cycle_t *init_cycle();

#endif /* _CYCLE_H_INCLUDED_ */

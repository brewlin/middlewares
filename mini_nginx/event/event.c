#include "event.h"
#include "cycle.h"
#include "log.h"

#define DEFAULT_CONNECTIONS  512

static int_t event_process_init(cycle_t *cycle);

module_t event_core_module = {
    NULL,
    NULL,
    event_process_init,
    NULL,NULL,NULL,NULL
};

uint_t            event_flags;
extern event_actions_t   event_actions;


uint_t            accept_events;
int_t             accept_disabled;

int_t
handle_read_event(event_t *rev, uint_t flags)
{
    log_info(GCYCLE->log,"event: handle_read_event");
    if (event_flags & USE_CLEAR_EVENT) {

        if (!rev->active && !rev->ready) {
            if (add_event(rev, READ_EVENT, CLEAR_EVENT)
                == ERROR)
            {
                return ERROR;
            }
        }
        return OK;

    }
    return OK;
}


int_t
handle_write_event(event_t *wev, size_t lowat)
{
    connection_t  *c = (connection_t*)wev->data;
    log_info(c->log,"event: handle write event");

    if (event_flags & USE_CLEAR_EVENT) {

        /* kqueue, epoll */

        if (!wev->active && !wev->ready) {
            if (add_event(wev, WRITE_EVENT,
                              CLEAR_EVENT | (lowat ? LOWAT_EVENT : 0))
                == ERROR)
            {
                return ERROR;
            }
        }

        return OK;

    } 


    return OK;
}




static int_t
event_process_init(cycle_t *cycle)
{
    log_info(cycle->log,"event: event_process_init");
    uint_t           i;
    event_t         *rev, *wev;
    listening_t     *ls;
    connection_t    *c, *next;
    cycle->connection_n = 100;

    //init epoll_create epoll_events
    event_actions.init(cycle,cycle->connection_n/2);

    //allocate connection pool
    cycle->connections = (connection_t *)malloc(sizeof(connection_t) * cycle->connection_n);
    if (cycle->connections == NULL) {
        return ERROR;
    }

    c = cycle->connections;

    //allocate read events pool
    cycle->read_events = malloc(sizeof(event_t) * cycle->connection_n);
    if (cycle->read_events == NULL) {
        return ERROR;
    }

    rev = cycle->read_events;
    for (i = 0; i < cycle->connection_n; i++) {
        rev[i].log = cycle->log;
        rev[i].closed = 1;
        rev[i].instance = 1;
    }
    //allocate write events pool
    cycle->write_events = (event_t *)malloc(sizeof(event_t) * cycle->connection_n);
    if (cycle->write_events == NULL) {
        return ERROR;
    }

    wev = cycle->write_events;
    for (i = 0; i < cycle->connection_n; i++) {
        wev[i].log = cycle->log;
        wev[i].closed = 1;
    }

    i = cycle->connection_n;
    next = NULL;

    //related connection - read event - write event
    do {
        i--;
        c[i].log  = cycle->log;
        c[i].data = next;
        c[i].read = &cycle->read_events[i];
        c[i].write = &cycle->write_events[i];
        c[i].fd = (socket_t) -1;

        next = &c[i];

    } while (i);

    cycle->free_connections = next;
    cycle->free_connection_n = cycle->connection_n;

    /* for each listening socket */
    // pack the listening_t to connection
    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

        c = get_connection(ls[i].fd);

        if (c == NULL) {
            return ERROR;
        }


        c->listening = &ls[i];
        ls[i].connection = c;

        rev = c->read;

        rev->accept = 1;

        rev->handler = event_accept;


        if (add_event(rev, READ_EVENT, 0) == ERROR) {
            return ERROR;
         }


    }

    return OK;
}



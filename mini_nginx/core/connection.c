#include "connection.h"
#include "mem_pool.h"
#include "config.h"
#include "cycle.h"
#include "array.h"
#include "event.h"
#include "log.h"

listening_t *create_listening(cycle_t *cycle, void *sockaddr, socklen_t socklen)
{
    listening_t  *ls;
    struct sockaddr  *sa;

    ls = array_push(&cycle->listening);
    if (ls == NULL) {
        return NULL;
    }

    memzero(ls, sizeof(listening_t));

    sa = palloc(cycle->pool, socklen);
    if (sa == NULL) {
        return NULL;
    }

    memcpy(sa, sockaddr, socklen);

    ls->sockaddr = sa;
    ls->socklen = socklen;

    ls->fd = (socket_t) -1;
    ls->type = SOCK_STREAM;

    ls->backlog = BACKLOG;
    ls->rcvbuf = -1;
    ls->sndbuf = -1;

    return ls;
}


int_t open_listening_sockets(cycle_t *cycle)
{
    int               reuseaddr;
    uint_t        i, tries, failed;
    socket_t      s;
    listening_t  *ls;

    reuseaddr = 1;

    /* TODO: configurable try number */

    for (tries = 2; tries; tries--) {
        failed = 0;

        /* for each listening socket */

        ls = cycle->listening.elts;
        for (i = 0; i < cycle->listening.nelts; i++) {

            if (ls[i].ignore) {
                continue;
            }

            if (ls[i].fd != -1) {
                continue;
            }

            s = socket(ls[i].sockaddr->sa_family, ls[i].type, 0);
            if(s == -1){
                log_info(cycle->log, "socket failed: %s ", strerror(errno));
                return ERROR;
            }
            log_info(cycle->log, "socket success: %s", strerror(errno));

            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,(const void *) &reuseaddr, sizeof(int)) == -1)
            {
                if (close_socket(s) == -1) {
                }

                return ERROR;
            }
            log_info(cycle->log, "setsockopt success: %s", strerror(errno));

            /* TODO: close on exit */

            // if (!(event_flags & USE_AIO_EVENT)) {
                if (nonblocking(s) == -1) {
                    if (close_socket(s) == -1) {
                    }

                    return ERROR;
                }
                log_info(cycle->log, "nonblocking success: %s", strerror(errno));
            // }

            if(bind(s, ls[i].sockaddr, ls[i].socklen) == -1) {
                log_error(cycle->log, "bind: %s", strerror(errno));

                if (close_socket(s) == -1) {
                }

                failed = 1;
                continue;
            }
            log_info(cycle->log, "bind success: %s", strerror(errno));


            if (listen(s, ls[i].backlog) == -1) {
                log_error(cycle->log, "listen: %s", strerror(errno));

                if (close_socket(s) == -1) {
                }

                return ERROR;
            }

            ls[i].listen = 1;
            ls[i].fd = s;
        }

        if (!failed) {
            break;
        }

        /* TODO: delay configurable */
        log_info(cycle->log,"try again to bind() after 500ms");
        msleep(500);
    }

    if (failed) {
        log_error(cycle->log,"still could not bind");
        return ERROR;
    }

    return OK;
}


void close_listening_sockets(cycle_t *cycle)
{
    uint_t         i;
    listening_t   *ls;
    connection_t  *c;


    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

        c = ls[i].connection;

        if (c) {
            if (c->read->active) {


                    /*
                     * it seems that Linux-2.6.x OpenVZ sends events
                     * for closed shared listening sockets unless
                     * the events was explicity deleted
                     */

                del_event(c->read, READ_EVENT, 0);
            }

            free_connection(c);

            c->fd = (socket_t) -1;
        }

        log_info(c->log,"close listening #%d ", ls[i].fd);

        if (close(ls[i].fd) == -1) {
            log_info(c->log,"close listening #%d failed", ls[i].fd);
        }
        ls[i].fd = (socket_t) -1;
    }
}


connection_t *get_connection(socket_t s)
{
    uint_t         instance;
    event_t       *rev, *wev;
    connection_t  *c;

    c = GCYCLE->free_connections;
    if (c == NULL) {
        log_error(GCYCLE->log,"%d worker_connections are not enough", GCYCLE->connection_n);
        return NULL;
    }

    GCYCLE->free_connections = c->data;
    GCYCLE->free_connection_n--;

    rev = c->read;
    wev = c->write;

    memzero(c, sizeof(connection_t));

    c->read = rev;
    c->write = wev;
    c->fd = s;
    c->log = GCYCLE->log;

    instance = rev->instance;

    memzero(rev, sizeof(event_t));
    memzero(wev, sizeof(event_t));

    rev->instance = !instance;
    wev->instance = !instance;

    rev->data = c;
    wev->data = c;

    wev->write = 1;

    return c;
}


void
free_connection(connection_t *c)
{
    cycle_t *cycle = GCYCLE;

    c->data = cycle->free_connections;
    cycle->free_connections = c;
    cycle->free_connection_n++;
}


void
close_connection(connection_t *c)
{
    socket_t  fd;

    if (c->fd == -1) {
        log_error(c->log, "connection already closed");
        return;
    }

    if (del_conn) {
        del_conn(c, CLOSE_EVENT);

    } else {
        if (c->read->active) {
            del_event(c->read, READ_EVENT, CLOSE_EVENT);
        }

        if (c->write->active) {
            del_event(c->write, WRITE_EVENT, CLOSE_EVENT);
        }
    }

    free_connection(c);

    fd = c->fd;
    c->fd = (socket_t) -1;

    if (close_socket(fd) == -1) {
        log_error(c->log,"close socket failed");
    }
}

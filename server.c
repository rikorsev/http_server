#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>

#include "server.h"
#include "tls.h"
#include "soc.h"
#include "config.h"
#include "log.h"

#define MODULE_NAME "server"

struct server_s *server_create(bool is_secure)
{
    struct server_s *srv = NULL;

    /* Allocate mamory for server */
    srv = malloc(sizeof(struct server_s));
    if (srv == NULL)
    {
        LOGERR("Fail allocate memory for server object");

        return NULL;
    }

    /* Get server context and interface depend on secure option */
    if (is_secure == true)
    {
        srv->ctx = tls_serv_ctx_alloc();
        srv->iface = tls_serv_iface_get();
    }
    else
    {
        srv->ctx = soc_serv_ctx_alloc();
        srv->iface = soc_serv_iface_get();
    }

    if (srv->ctx == NULL)
    {
        LOGERR("Fail allocate memory for server context object");

        free(srv);

        return NULL;
    }

    if (srv->iface == NULL)
    {
        LOGERR("Fail to get server interface");

        free(srv->ctx);
        free(srv);

        return NULL;
    }

    return srv;
}

int server_init(struct server_s *srv, char *addr, int port)
{
    if (srv == NULL)
    {
        LOGERR("Invalid argument");

        return -EINVAL;
    }

    if (srv->iface->init == NULL)
    {
        LOGERR("Not implemented");

        return -ENOSYS;
    }

    return srv->iface->init(srv->ctx, addr, port);
}

static void server_conn_close(struct conn_s *conn)
{

    if (conn->iface->close != NULL)
    {
        conn->iface->close(conn->ctx);
    }

    free(conn);
}

static void *server_conn_handler(void *data)
{
    struct conn_s *conn = (struct conn_s *)data;
    size_t len = 0;
    char buf[CONFIG_INPUT_BUFF_LEN] = {0}; /** @todo: data chunking */
    int keepalive = 0;

    if (conn == NULL)
    {
        LOGERR("Connection object is NULL");

        pthread_exit(NULL);
    }

    if (conn->iface->recv == NULL)
    {
        LOGERR("Read interface is not implemented");

        goto exit;
    }

    /* Recive data */
    do
    {
        len = conn->iface->recv(conn->ctx, buf, sizeof(buf));
        if (len < 0)
        {
            LOGERR("Fail to receive data. Result: %s", strerror(errno));

            break;
        }

        /* Handle received data */
        keepalive = conn->handler(conn, buf, len);
    } while (keepalive > 0);

exit:
    server_conn_close(conn);

    /* Exit the thread */
    pthread_exit(NULL);

    return NULL;
}

int server_listen(struct server_s *srv, server_listen_handler_f handler)
{
    void *connctx = NULL;
    struct conn_s *conn = NULL;
    struct conn_iface_s *conn_iface = NULL;
    int result = 0;
    pthread_t thread = 0;

    /* Check if arguments are valid */
    if (srv == NULL || handler == NULL)
    {
        LOGERR("Invalid argument");

        return -EINVAL;
    }

    /* Check if required interface is available */
    if (srv->iface->accept == NULL ||
        srv->iface->conn_iface == NULL)
    {
        LOGERR("Required interfaces are not implemented");

        return -ENOSYS;
    }

    /* Try to get connection interface in advance */
    conn_iface = srv->iface->conn_iface();
    if (conn_iface == NULL)
    {
        LOGERR("Fail to get connectio interface");

        return -ENOSYS;
    }

    while (1)
    {
        /* Wait for new connection */
        connctx = srv->iface->accept(srv->ctx);
        if (connctx == NULL)
        {
            LOGERR("Connection fail");

            continue;
        }

        /* Create new connection data */
        conn = malloc(sizeof(struct conn_s));
        if (conn == NULL)
        {
            LOGERR("Fail to allocate memory for new connection");

            free(connctx);

            continue;
        }

        conn->ctx = connctx;
        conn->handler = handler;
        conn->iface = conn_iface;

        /* Create separate thread for connection */
        result = pthread_create(&thread, NULL, server_conn_handler, conn);
        if (result < 0)
        {
            LOGERR("Fail to create new connection thread. Result %d", result);

            /* Close connection */
            server_conn_close(conn);

            continue;
        }
    }

    return 0;
}

int server_send(struct conn_s *conn, void *buf, size_t len)
{
    if (conn->iface->send == NULL)
    {
        LOGERR("Write interface is not implemented");

        return -ENOSYS;
    }

    int sendlen = conn->iface->send(conn->ctx, buf, len);
    if (sendlen < 0)
    {
        LOGERR("Fail to send. Result: %s", strerror(errno));

        return -errno;
    }

    return sendlen;
}

int server_close(struct server_s *srv)
{
    /** @todo: close all open threads ? */

    if (srv->iface->deinit == NULL)
    {
        LOGERR("Deinit interface is not implemented");

        return -ENOSYS;
    }

    srv->iface->deinit(srv->ctx);

    return 0;
}
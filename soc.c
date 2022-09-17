#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#include "server.h"
#include "config.h"
#include "log.h"

#define MODULE_NAME "socket"

struct servctx_s
{
    int sockfd;
};

struct connctx_s
{
    int connfd;
};

static int soc_init(void *ctx, char *addr, int port)
{
    struct servctx_s *servctx = (struct servctx_s *) ctx;
    struct sockaddr_in sockaddr = {0};

    /** @todo: address validation */

    if(ctx == NULL || addr == NULL)
    {
        LOGERR("Invalid argument");

        return -EINVAL;
    }

    /* Create socket */
    servctx->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(servctx->sockfd < 0)
    {
        LOGERR("Fail to create socket. Result: %s", strerror(errno));

        return -errno;
    }

    /* Set socket options reuse address to true */
    if(setsockopt(servctx->sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        LOGERR("Fail to set socket reuseaddr option. Result: %s", strerror(errno));

        return -errno;
    }

    /* Set address */
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    if(inet_aton(addr, (struct in_addr *)&sockaddr.sin_addr.s_addr) < 0)
    {
        LOGERR("Fail set address. Result: %s", strerror(errno));

        return -errno;
    }

    /* Bind socket and address */
    if(bind(servctx->sockfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0)
    {
        LOGERR("Fail to bind. Result: %s", strerror(errno));

        return -errno;
    }

    /* Start listening */
    if (listen(servctx->sockfd, SOMAXCONN) < 0)
    {
        LOGERR("Fail to listen. Result: %s\r\n", strerror(errno));

        return -errno;
    }

    LOGINF("Has been started. Address %s, port %d", addr, port);

    return 0;
}

static void *soc_accept(void *ctx)
{
    struct servctx_s *servctx = (struct servctx_s *) ctx;
    struct timeval tv = { .tv_sec = CONFIG_KEEPALIVE_TIMEOUT_SEC, .tv_usec = 0 };
    int conn = 0;

    if(ctx == NULL)
    {
        LOGERR("Invalid argument");

        return NULL;
    }

    /* Wait for new connection */
    conn = accept(servctx->sockfd, NULL, NULL);
    if(conn < 0)
    {
        LOGERR("Connection fail. Result: %s", strerror(errno));

        return NULL;
    }

    LOGINF("New connection %d", conn);

    /* Set connection timout for recv call */
    if(setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        LOGERR("Fail to set connection rcvtimeo option. Result: %s", strerror(errno));

        return NULL;
    }

    /* Create connection context object */
    struct connctx_s *connctx = malloc(sizeof(struct connctx_s));
    if(connctx == NULL)
    {
        LOGERR("Fail to allocate memory for connection context");

        return NULL;
    }

    connctx->connfd = conn;

    return connctx;
}

static void soc_deinit(void *ctx)
{
    struct servctx_s *servctx = (struct servctx_s *) ctx;

    if(ctx == NULL)
    {
        LOGERR("Invalid argument");

        return;
    }

    /* Close socket */
    if(close(servctx->sockfd) < 0)
    {
        LOGERR("Fail close socket. Result: %s", strerror(errno));

        return;
    }
}

static int soc_recv(void *ctx, char *buf, size_t len)
{
    struct connctx_s *connctx = (struct connctx_s *) ctx;

    if(ctx == NULL || buf == NULL)
    {
        LOGERR("Invalid argument");

        return -EINVAL;
    }

    len = recv(connctx->connfd, buf, len, 0);
    if((ssize_t)len < 0)
    {
        LOGERR("Fail to receive data. Result: %s", strerror(errno));

        return -errno;
    }

    return len;
}

static int soc_send(void *ctx, char *buf, size_t len)
{
    struct connctx_s *connctx = (struct connctx_s *) ctx;

    if(ctx == NULL || buf == NULL)
    {
        LOGERR("Invalid argument");

        return -EINVAL;
    }

    len = send(connctx->connfd, buf, len, 0);
    if((ssize_t)len < 0)
    {
        LOGERR("Fail to send. Result: %s", strerror(errno));

        return -errno;
    }

    return len;
}

static void soc_conn_close(void *ctx)
{
    struct connctx_s *connctx = (struct connctx_s *) ctx;

    if(ctx == NULL)
    {
        LOGERR("Invalid argument");

        return;
    }

    LOGINF("Connection %d closed", connctx->connfd);

    if(close(connctx->connfd) < 0)
    {
        LOGERR("Fail close connection. Result: %s", strerror(errno));
    }

    free(connctx);
}

const static struct conn_iface_s conn_iface =
{
    .recv  = soc_recv,
    .send  = soc_send,
    .close = soc_conn_close
};

static struct conn_iface_s *soc_conn_iface_get(void)
{
    return (struct conn_iface_s *)&conn_iface;
}

const static struct server_iface_s serv_iface =
{
    .init       = soc_init,
    .accept     = soc_accept,
    .deinit     = soc_deinit,
    .conn_iface = soc_conn_iface_get
};

struct server_iface_s *soc_serv_iface_get(void)
{
    return (struct server_iface_s *)&serv_iface;
}

void *soc_serv_ctx_alloc(void)
{
    struct servctx_s *servctx = NULL;

    servctx = malloc(sizeof(struct servctx_s));
    if(servctx == NULL)
    {
        LOGERR("Fail to allocate memory for server context");

        return NULL;
    }

    return servctx;
}

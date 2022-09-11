#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#include <pthread.h>

#include "server.h"
#include "config.h"
#include "log.h"

#define MODULE_NAME "server"

struct conn_data_s {
    int conn;
    server_listen_handler_f handler;
};

int server_create(char *addr, int port)
{
    int sockfd = 0;
    struct sockaddr_in sockaddr = {0};

    /** @todo: address validation */

    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        LOGERR("Fail to create socket. Result: %s", strerror(errno));

        return -errno;
    }

    /* Set socket options reuse address to true */
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
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
    if(bind(sockfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0)
    {
        LOGERR("Fail to bind. Result: %s", strerror(errno));

        return -errno;
    }

    /* Start listening */
    if (listen(sockfd, SOMAXCONN) < 0)
    {
        LOGERR("Fail to listen. Result: %s\r\n", strerror(errno));

        return -errno;
    }

    LOGINF("Has been started. Address %s, port %d", addr, port);

    return sockfd;
}

static void server_conn_close(struct conn_data_s *conn_data)
{
    LOGINF("Connection %d closed", conn_data->conn);

    if(close(conn_data->conn) < 0)
    {
        LOGERR("Fail close connection. Result: %s", strerror(errno));
    }

    free(conn_data);
}

static void *server_conn_handler(void *data)
{
    struct conn_data_s *conn_data = (struct conn_data_s *) data;
    ssize_t received = 0;
    char buf[CONFIG_INPUT_BUFF_LEN] = {0}; /** @todo: data chunking */
    int keepalive = 0;

    /* Recive data */
    do
    {
        received = recv(conn_data->conn, buf, sizeof(buf), 0);
        if(received < 0)
        {
            LOGERR("Fail to receive data. Result: %s", strerror(errno));

            break;
        }

        /* Handle received data */
        keepalive = conn_data->handler(conn_data->conn, buf, received);
    } while(keepalive > 0);

    server_conn_close(conn_data);

    /* Exit the thread */
    pthread_exit(NULL);

    return NULL;
}

int server_listen(int sockfd, server_listen_handler_f handler)
{
    int conn = 0;
    int result = 0;
    pthread_t thread = 0;
    struct timeval tv = { .tv_sec = CONFIG_KEEPALIVE_TIMEOUT_SEC, .tv_usec = 0 };

    /* Check if handler function is valid */
    if(handler == NULL)
    {
        LOGERR("Data handler function is NULL");

        return -EINVAL;
    }

    while(1)
    {
        /* Wait for new connection */
        conn = accept(sockfd, NULL, NULL);
        if(conn < 0)
        {
            LOGERR("Connection fail. Result: %s", strerror(errno));

            return -errno;
        }

        LOGINF("New connection %d", conn);

        /* Set connection timout for recv call */
        if(setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            LOGERR("Fail to set connection rcvtimeo option. Result: %s", strerror(errno));

            return -errno;
        }

        /* Create new connection data structure */
        struct conn_data_s *conn_data = malloc(sizeof(struct conn_data_s));
        if(conn_data == NULL)
        {
            LOGERR("Connection data allocation fail");

            return -ENOMEM;
        }

        /* Set conn data */
        conn_data->conn = conn;
        conn_data->handler = handler;

        /* Create separate thread for connection */
        result = pthread_create(&thread, NULL, server_conn_handler, conn_data);
        if(result < 0)
        {
            LOGERR("Fail to create new connection thread. Result %d", result);

            /* Close connection */
            server_conn_close(conn_data);

            return result;
        }
    }

    return 0;
}

int server_send(int conn, void *buf, size_t len)
{
    int sendlen = send(conn, buf, len, 0);
    if(sendlen < 0)
    {
        LOGERR("Fail to send. Result: %s", strerror(errno));

        return -errno;
    }

    return sendlen;
}

int server_close(int sockfd)
{
    /** @todo: close all open threads ? */

    /* Close socket */
    if(close(sockfd) < 0)
    {
        LOGERR("Fail close socket. Result: %s", strerror(errno));

        return -errno;
    }

    return 0;
}
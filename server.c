#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <pthread.h>

struct conn_data_s {
    int conn;
    void (*handler_func)(int conn, char *buf, size_t len);
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
        fprintf(stderr, "server: Fail to create socket. Result: %s\r\n", strerror(errno));

        return -errno;
    }

    /* Set socket options */
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        fprintf(stderr, "server: Fail to set socket options. Result: %s\r\n", strerror(errno));

        return -errno;
    }

    /* Set address */
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(80);
    if(inet_aton(addr, &sockaddr.sin_addr.s_addr) < 0)
    {
        fprintf(stderr, "server: Fail set address. Result: %s\r\n", strerror(errno));

        return -errno;
    }

    /* Bind socket and address */
    if(bind(sockfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0)
    {
        fprintf(stderr, "server: Fail to bind. Result: %s\r\n", strerror(errno));

        return -errno;
    }

    /* Start listening */
    if (listen(sockfd, SOMAXCONN) < 0)
    {
        fprintf(stderr, "server: Fail to listen. Result: %s\r\n", strerror(errno));

        return -errno;
    }

    printf("server: has been started. Address %s, port %d...\r\n", addr, port);

    return sockfd;
}

static void server_conn_close(struct conn_data_s *conn_data)
{
    if(close(conn_data->conn) < 0)
    {
        fprintf(stderr, "server: Fail close connection. Result: %s\r\n", strerror(errno));
    }

    free(conn_data);
}

static void server_conn_handler(void *data)
{
    struct conn_data_s *conn_data = (struct conn_data_s *) data;
    ssize_t received = 0;
    char buf[1024] = {0}; /** @todo: data chunking */

    /* Recive data */
    received = recv(conn_data->conn, buf, sizeof(buf), 0);
    if(received < 0)
    {
        fprintf(stderr, "server: Fail to receive data. Result: %s\r\n", strerror(errno));

        goto exit;
    }

    /* Handle received data */
    conn_data->handler_func(conn_data->conn, buf, received);

exit:

    /* Close connection */
    server_conn_close(conn_data);

    /* Exit the thread */
    pthread_exit(NULL);

}

int server_listen(int sockfd, void (*handler_func)(int conn, char *buf, size_t len))
{
    int conn = 0;
    int result = 0;
    pthread_t thread = 0;

    /* Check if handler function is valid */
    if(handler_func == NULL)
    {
        fprintf(stderr, "server: Data handler function is NULL\r\n");

        return -1;
    }

    while(1)
    {
        /* Wait for new connection */
        conn = accept(sockfd, NULL, NULL);
        if(conn < 0)
        {
            fprintf(stderr, "Connection fail. Result: %s\r\n", strerror(errno));

            return -errno;
        }

        /* Create new connection data structure */
        struct conn_data_s *conn_data = malloc(sizeof(struct conn_data_s));
        if(conn_data == NULL)
        {
            fprintf(stderr, "Connection data allocation fail");

            return -1;
        }

        /* Set conn data */
        conn_data->conn = conn;
        conn_data->handler_func = handler_func;

        /* Create separate thread for connection */
        result = pthread_create(&thread, NULL, server_conn_handler, conn_data);
        if(result < 0)
        {
            fprintf(stderr, "server: Fail to create new connection thread. Result %d", result);

            /* Close connection */
            server_conn_close(conn_data);

            return -1;
        }
    }

    return 0;
}

int server_send(int conn, char *buf, size_t len)
{
    int sendlen = send(conn, buf, len, 0);
    if(sendlen < 0)
    {
        fprintf(stderr, "Fail to send index.html. Result: %s\r\n", strerror(errno));

        return -errno;
    }

    return sendlen;
}

int server_close(int sockfd)
{
    /* Close socket */
    if(close(sockfd) < 0)
    {
        fprintf(stderr, "server: Fail close socket. Result: %s\r\n", strerror(errno));

        return -errno;
    }

    return 0;
}
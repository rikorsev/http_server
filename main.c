#include <stdio.h>

#include "server.h"
#include "http.h"

void http_index_get(int conn)
{
    #define RESPBUF_LEN 1024

    char respbuf[RESPBUF_LEN];
    int sendlen = 0;
    int readlen = 0;
    FILE *file = NULL;

    if(http_send_ok(conn) < 0)
    {
        fprintf(stderr, "main: Fail to send ok responce\r\n");

        return;
    }

    /* Open index file */
    file = fopen("index.html", "r");
    if (file == NULL) 
    {
        fprintf(stderr, "main: Fail to open index.html\r\n");

        return;
    }

    /* Send index file */
    while((readlen = fread(respbuf, sizeof(char), RESPBUF_LEN, file)) != 0)
    {
        sendlen = server_send(conn, respbuf, readlen);
        if(sendlen < 0)
        {
            fprintf(stderr, "main: Fail to send index\r\n");

            return;
        }
    }

    fclose(file);
}

void http_index_handler(int conn, enum method_e method, char *buf, size_t len)
{
    switch(method)
    {
        case METHOD_GET:
            http_index_get(conn);
            break;

        default:
            fprintf(stderr, "http: Wrong method for the resource\r\n");

            http_send_not_implemented(conn);

    }
}

static int start_server(char *addr, int port, server_listen_handler_f handler, void *meta, size_t metalen)
{
    int sockfd = 0;
    sockfd = server_create(addr, port);
    if(sockfd < 0)
    {
        fprintf(stderr, "main: Fail to create new server. Result %d\r\n", sockfd);

        return sockfd;
    }

    if(server_listen(sockfd, handler, meta, metalen) < 0)
    {
        fprintf(stderr, "main: Fail listen\r\n");

        server_close(sockfd);

        return -1;
    }

    return 0;
}

int main(void)
{
    /* Create http table */
    #define NUM_OF_HTTP_RESOURCES 5
    struct http_table_s *http_table = http_table_create(NUM_OF_HTTP_RESOURCES);
    if(http_table == NULL)
    {
        fprintf(stderr, "main: Fail to create HTTP table\r\n");

        return -1;
    }

    /* Fill it with resources */
    if(http_table_register_resource(http_table, "/", http_index_handler) < 0)
    {
        fprintf(stderr, "main: Fail to register resource\r\n");

        http_table_free(http_table);

        return -1;
    }

    if(http_table_register_resource(http_table, "/index.html", http_index_handler) < 0)
    {
        fprintf(stderr, "main: Fail to register resource\r\n");

        http_table_free(http_table);

        return -1;
    }

    /* Start server */
    if(start_server("127.0.0.1", 80, http_handler, http_table, sizeof(struct http_table_s)) < 0)
    {
        fprintf(stderr, "main: fail to start server\r\n");

        return -1;
    }

    return 0;
}
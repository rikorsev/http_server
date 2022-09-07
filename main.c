#include <stdio.h>

#include "server.h"

#define RESPBUF_LEN 1024

static void handler(int conn, char *buf, size_t len)
{
    char respbuf[RESPBUF_LEN] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
    int sendlen = 0;
    int readlen = 0;
    FILE *file = NULL;

    printf("new data for %d connection. Len: %ld. Content:\r\n%s", conn, len, buf);

    /* Send responce header */
    sendlen = server_send(conn, respbuf, sizeof(respbuf));
    if(sendlen < 0)
    {
        fprintf(stderr, "main: Fail to send responce\r\n");

        return;
    }

    file = fopen("index.html", "r");
    if (file == NULL) 
    {
        fprintf(stderr, "main: Fail to open index.html\r\n");

        return;
    }

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

int main(void)
{
    int sockfd = 0;
    sockfd = server_create("127.0.0.1", 80);
    if(sockfd < 0)
    {
        fprintf(stderr, "main: Fail to create new server. Result %d\r\n", sockfd);

        return sockfd;
    }

    if(server_listen(sockfd, handler) < 0)
    {
        fprintf(stderr, "main: Fail listen\r\n");

        server_close(sockfd);

        return -1;
    }

    return 0;
}
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(void)
{
    int sockfd = 0;
    int result = 0;
    int conn = 0;
    struct sockaddr_in addr = {0};
    char buf[2048] = {0};
    ssize_t received = 0;
    ssize_t sended = 0;

    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        fprintf(stderr, "Fail to create socket. Result: %s\r\n", strerror(errno));

        return errno;
    }

    /* Set socket options */
    result = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if(result < 0)
    {
        fprintf(stderr, "Fail to set socket options. Result: %s\r\n", strerror(errno));

        return errno;
    }

    /* Set address */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    result = inet_aton("127.0.0.1", &addr.sin_addr.s_addr);
    if(result < 0)
    {
        fprintf(stderr, "Fail set address. Result: %s\r\n", strerror(errno));

        return errno;
    }

    /* Bind socket and address */
    result = bind(sockfd, (struct sockaddr *) &addr, sizeof(addr));
    if(result < 0)
    {
        fprintf(stderr, "Fail to bind. Result: %s\r\n", strerror(errno));

        return errno;
    }

    /* Start listening */
    result = listen(sockfd, SOMAXCONN);
    if (result != 0)
    {
        fprintf(stderr, "Fail to listen. Result: %s\r\n", strerror(errno));

        return errno;
    }

    printf("Listening...\r\n");

    while(1)
    {
        /* Wait for new connection */
        conn = accept(sockfd, NULL, NULL);
        if(conn < 0)
        {
            fprintf(stderr, "Connection fail. Result: %s\r\n", strerror(errno));

            return errno;
        }

        /* Recive data */
        received = recv(conn, buf, sizeof(buf), 0);
        if(received < 0)
        {
            fprintf(stderr, "Fail to receive data. Result: %s\r\n", strerror(errno));

            return errno;
        }

        printf("%ld bytes received\r\n", received);
        printf("Data: %s", buf);

        /* Send responce */
        char resp[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
        sended = send(conn, resp, sizeof(resp), 0);
        if(sended < 0)
        {
            fprintf(stderr, "Fail to send responce. Result: %s\r\n", strerror(errno));

            return errno;
        }

        printf("Sent responce %ld bytes\r\n", sended);

        /* Send header */
        FILE *file = fopen("./index.html", "r");
        if (file == NULL) 
        {
            fprintf(stderr, "Fail to open index.html. Result: %s\r\n", strerror(errno));

            return -1;
        }

        /* Send file content */
        size_t readsize = 0;
        char buffer[256] = {0};

        while((readsize = fread(buffer, sizeof(char), BUFSIZ, file)) != 0) 
        {
            sended = send(conn, buffer, readsize, 0);
            if(sended < 0)
            {
                fprintf(stderr, "Fail to send index.html. Result: %s\r\n", strerror(errno));

                return errno;
            }
        }

        fclose(file);

        /* Close connection */
        result = close(conn);
        if(result < 0)
        {
            fprintf(stderr, "Fail close connection. Result: %s\r\n", strerror(errno));

            return errno;
        }
    }

    /* Close socket */
    result = close(sockfd);
    if(result < 0)
    {
        fprintf(stderr, "Fail close socket. Result: %s\r\n", strerror(errno));

        return errno;
    }

    return 0;
}
#ifndef SERVER_H_
#define SERVER_H_

typedef void (*server_listen_handler_f)(int conn, char *buf, size_t len);

int server_create(char *addr, int port);
int server_listen(int sockfd, server_listen_handler_f handler);
int server_send(int conn, char *buf, size_t len);
int server_close(int sockfd);

#endif
#ifndef SERVER_H_
#define SERVER_H_

int server_create(char *addr, int port);
int server_listen(int sockfd, void (*handler_func)(int conn, char *buf, size_t len));
int server_send(int conn, char *buf, size_t len);
int server_close(int sockfd);

#endif
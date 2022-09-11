/**
 * @file server.h
 * @brief This module responsible for maintaining communication via sockets
 * 
 * The module do following:
 *  - create bind and listen socket, 
 *  - wait for new connection from client and create sepatate thread to handle it
 *  - pass recived data to upper layer/protocol (HTTP, etc)
 *  - provide interface to send a data to client
 **/

#ifndef SERVER_H_
#define SERVER_H_

/**
 * @brief Function handler type that uses to handle incoming data 
 * on top protocol layers such as HTTP, etc.
 * 
 * @param conn[in] - connection descriptor
 * @param buf[in] - buffer with incoming data
 * @param len[in] - lengh of incoming data
 * 
 * @retval shall return 1 to keep connection open after handling the request
 * or 0 othervise
 **/
typedef int (*server_listen_handler_f)(int conn, char *buf, size_t len);

/**
 * @brief Creates new listener for server based on socket
 * 
 * The function open new socket, bind it with specefied address and port 
 * and start listening
 * 
 * @param addr[in] - server address
 * @param port[in] - server potr
 * 
 * @retval opened socket dile descriptor if everything is fine.
 * Errno value if something goes wrong.
 **/
int server_create(char *addr, int port);

/**
 * @brief Listen for new connections
 * 
 * The function listen for new connections in infinite loop. 
 * If new connection occures it creates separate thread to handle incoming data
 * 
 * @param sockfd[in] - socket file descriptor.
 * @param handler[in] - handler function that should handle incoming data on upper layer
 * 
 * @retval The function should not return back if everything is ok.
 * If something goes wrong it returns negative errno value
 * 
 * @warning BE AWARE that the function shall not return after it
 * call according to normal flow.
 **/
int server_listen(int sockfd, server_listen_handler_f handler);

/**
 * @brief Send data to client
 * 
 * The function sends data to specific client according to provided connection descriptor
 * 
 * @param conn[in] - connection descriptor
 * @param buf[in] - bufer that contains data to send
 * @param len[in] - length of data to send
 * 
 * @retval num of sent bytes if everything is ok,
 * errno value in case of error
 **/
int server_send(int conn, void *buf, size_t len);

/**
 * @brief Close server
 * 
 * @param sockfd[in] - socket file descriptor
 * 
 * @retval zero if case if success, errno value in case of error
 **/
int server_close(int sockfd);

#endif
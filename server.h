/**
 * @file server.h
 * @brief This module responsible for maintaining communication via sockets
 * 
 * The module do following:
 *  - create server object/context and init it
 *  - wait for new connection from client and create sepatate thread to handle it
 *  - pass recived data to upper layer/protocol (HTTP, etc)
 *  - provide interface to send a data to client
 **/

#ifndef SERVER_H_
#define SERVER_H_

#include <stdlib.h>
#include <stdbool.h>

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
typedef int (*server_listen_handler_f)(void *connctx, char *buf, size_t len);

/**
 * @brief The structure represents connection interface for lower layers like socket/TLS
 * 
 **/
struct conn_iface_s
{
    /**
     * @brief Interface to recive data via connection channel
     * 
     * @param connctx[in] - connection context
     * @param buf[in] - buffer to receive data
     * @param len[in] - the buffer lenght
     * 
     * @retval number of read bytes in case of success, negative value otherwise
     **/
    int (*recv)(void *connctx, char *buf, size_t len);

    /**
     * @brief Interface to send data via connection channel
     * 
     * @param connctx[in] - connection context
     * @param buf[in] - buffer to send data
     * @param len[in] - the buffer lenght
     * 
     * @retval number of sent bytes in case of success, negative value otherwise
     **/
    int (*send)(void *connctx, char *buf, size_t len);

    /**
     * @brief Interface to close connection channel
     * 
     * @param connctx[in] - connection context
     **/
    void (*close)(void *connctx);
};

/**
 * @brief The structure represents server interface for lower layers like sockets/TLS
 * 
 **/
struct server_iface_s
{
    /**
     * @brief Interface to init server, bind it with specified addres and port
     * 
     * @param server[in] - server context
     * @param addr[in] - IP address to assign
     * @param port[in] - TCP port to assign
     * 
     * @retval 0 in case of success, negative value otherwise
     **/
    int (*init)(void *server, char *addr, int port);

    /**
     * @brief Interface to wait or new connection
     * 
     * @param server[in] - server context
     * 
     * @retval pointer to connection context or NULL in case of error
     **/
    void *(*accept)(void *server);

    /**
     * @brief Interface to deinit close and free a server
     * 
     * @param server[in] - server context
     **/
    void (*deinit)(void *server);
    
    /**
     * @brief Interface to get connection interface
     * 
     * @retval return pointer to connection interface
     * 
     * @todo This interface looks not really suitable for the structure.
     * May be in future it would be better to implement getting of 
     * connection interface in other way
     **/
    struct conn_iface_s *(*conn_iface)(void);
};

/**
 * @brief The structure represents server context
 **/
struct server_s
{ 
    struct server_iface_s *iface; /// pointer to lower layer implementation server interace
    void *ctx;                    /// pointer to lower layer implementation server context data
};

/**
 * @brief The strucutre represents connection context
 **/
struct conn_s
{
    struct conn_iface_s *iface;      /// pointer to lower layer connection interface
    server_listen_handler_f handler; /// pointer higher layer data handler
    void *ctx;                       /// pointer to lower later connection context data
};

/**
 * @brief Creates new server object
 * 
 * New server object will be created based on is_secure option.
 * if is secure == true, the server will use TLS implementation,
 * otherwise regular sockets will be used.
 * 
 * @param is_secure[in] - choose which kind of server will be created,
 * secure(TLS) or nonsecure(sockets)
 * 
 * @retval pointer to server object(server context) or NULL in case of error
 **/
struct server_s *server_create(bool is_secure);

/**
 * @brief Init server object/context
 * 
 * @param srv[in]  - server object/context to init
 * @param addr[in] - server address
 * @param port[in] - server port
 * 
 * @retval 0 in case o success, negative value otherwise
 **/
int server_init(struct server_s *srv, char *addr, int port);

/**
 * @brief Listen for new connections
 * 
 * The function listen for new connections in infinite loop. 
 * If new connection occures, it creates separate thread to handle incoming data
 * 
 * @param srv[in]     - server object/context
 * @param handler[in] - handler function that should handle incoming data on upper layer
 * 
 * @retval The function should not return back if everything is ok.
 * If something goes wrong it returns negative errno value
 * 
 * @warning BE AWARE that the function shall not return after it
 * call according to normal flow.
 **/
int server_listen(struct server_s *srv, server_listen_handler_f handler);

/**
 * @brief Send data to client
 * 
 * The function sends data to specific client according to provided connection context
 * 
 * @param conn[in] - connection context
 * @param buf[in] - bufer that contains data to send
 * @param len[in] - length of data to send
 * 
 * @retval num of sent bytes if everything is ok,
 * errno value in case of error
 **/
int server_send(struct conn_s *conn, void *buf, size_t len);

/**
 * @brief Close server
 * 
 * Deinit close and free server resources
 * 
 * @param srv[in] - server object/context
 * 
 * @retval zero in case if success, errno value in case of error
 **/
int server_close(struct server_s *srv);

#endif
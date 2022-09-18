#ifndef TLS_H_
#define TLS_H_

#include "server.h"

/**
 * @brief Provide access to TLS server interface inction
 * 
 * @retval Pointer to structure that contains TLS server interfaces
 **/
struct server_iface_s *tls_serv_iface_get(void);

/**
 * @brief Allocates memory for TLS server context
 * 
 * @retval pointer to allocated server context data
 **/
void *tls_serv_ctx_alloc(void);

#endif
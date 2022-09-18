#ifndef SOC_H_
#define SOC_H_

#include "server.h"

/**
 * @brief Provide access to socket server interface inction
 * 
 * @retval Pointer to structure that contains socket server interfaces
 **/
struct server_iface_s *soc_serv_iface_get(void);

/**
 * @brief Allocates memory for socket server context
 * 
 * @retval pointer to allocated server context
 **/
void *soc_serv_ctx_alloc(void);

#endif
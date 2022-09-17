#ifndef SOC_H_
#define SOC_H_

#include "server.h"

struct server_iface_s *soc_serv_iface_get(void);
void *soc_serv_ctx_alloc(void);

#endif
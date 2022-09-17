#ifndef TLS_H_
#define TLS_H_

#include "server.h"

struct server_iface_s *tls_serv_iface_get(void);
void *tls_serv_ctx_alloc(void);

#endif
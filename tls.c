#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_cookie.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/timing.h"

#include "test/certs.h"

#include "server.h"
#include "config.h"
#include "log.h"

#define MODULE_NAME "tls"

struct servctx_s
{
    mbedtls_net_context listen_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
    mbedtls_ssl_cookie_ctx cookie_ctx;
};

struct connctx_s
{
    mbedtls_net_context client_fd;
    mbedtls_timing_delay_context timer;
    mbedtls_ssl_context ssl;
};

static char *tls_error(int error)
{
    static char error_buf[100];

    mbedtls_strerror(error, error_buf, 100);

    return error_buf;
}

static int tls_init(void *ctx, char *addr, int port)
{
    int result = 0;
    const char *pers = "ssl_server";
    char portstr[8] = {0};
    struct servctx_s *servctx = (struct servctx_s *) ctx;

    if(ctx == NULL || addr == NULL)
    {
        LOGERR("Invalid argument");

        return -EINVAL;
    }

    mbedtls_net_init(&servctx->listen_fd);
    mbedtls_ssl_config_init(&servctx->conf);
    mbedtls_x509_crt_init(&servctx->srvcert);
    mbedtls_pk_init(&servctx->pkey);
    mbedtls_entropy_init(&servctx->entropy);
    mbedtls_ctr_drbg_init(&servctx->ctr_drbg);
    mbedtls_ssl_cookie_init(&servctx->cookie_ctx);

    result =  mbedtls_ctr_drbg_seed(&servctx->ctr_drbg, mbedtls_entropy_func, &servctx->entropy,
                                   (const unsigned char *)pers, strlen(pers));
    if(result < 0)
    {
        LOGERR("Fail to seed random number generator. Result: %s", tls_error(result));

        return result;
    }

    /*
    * This demonstration program uses embedded test certificates.
    * Instead, you may want to use mbedtls_x509_crt_parse_file() to read the
    * server and CA certificates, as well as mbedtls_pk_parse_keyfile().
    */
    result = mbedtls_x509_crt_parse(&servctx->srvcert, (const unsigned char *)mbedtls_test_srv_crt,
                                    mbedtls_test_srv_crt_len);
    if(result < 0)
    {
        LOGERR("Fail to get server certificate. Result: %s", tls_error(result));

        return result;
    }

    result = mbedtls_x509_crt_parse(&servctx->srvcert, (const unsigned char *) mbedtls_test_cas_pem,
                                    mbedtls_test_cas_pem_len);
    if(result < 0)
    {
        LOGERR("Fail to get CA certificate. Result: %s", tls_error(result));

        return result;
    }

    result = mbedtls_pk_parse_key(&servctx->pkey, (const unsigned char *) mbedtls_test_srv_key,
                    mbedtls_test_srv_key_len, NULL, 0, mbedtls_ctr_drbg_random, &servctx->ctr_drbg);
    if(result < 0)
    {
        LOGERR("Fail to get server key. Result: %s", tls_error(result));

        return result;
    }

    sprintf(portstr, "%d", port);
    result = mbedtls_net_bind(&servctx->listen_fd, addr, portstr, MBEDTLS_NET_PROTO_TCP);
    if(result < 0)
    {
        LOGERR("Fail to bind net. Result: %s", tls_error(result));

        return result;
    }

    result = mbedtls_ssl_config_defaults(&servctx->conf, MBEDTLS_SSL_IS_SERVER,
                                                         MBEDTLS_SSL_TRANSPORT_STREAM,
                                                         MBEDTLS_SSL_PRESET_DEFAULT);
    if(result < 0)
    {
        LOGERR("Fail to set config defaults. Result: %s", tls_error(result));

        return result;
    }

    mbedtls_ssl_conf_rng(&servctx->conf, mbedtls_ctr_drbg_random, &servctx->ctr_drbg);
    mbedtls_ssl_conf_read_timeout(&servctx->conf, CONFIG_KEEPALIVE_TIMEOUT_SEC * 1000);

    mbedtls_ssl_conf_ca_chain(&servctx->conf, servctx->srvcert.next, NULL);
    result = mbedtls_ssl_conf_own_cert(&servctx->conf, &servctx->srvcert, &servctx->pkey);
    if(result < 0 )
    {
        LOGERR("Fail to set own certificate. Result: %s", tls_error(result));

        return result;
    }

    result = mbedtls_ssl_cookie_setup(&servctx->cookie_ctx, mbedtls_ctr_drbg_random, 
                                      &servctx->ctr_drbg);
    if(result < 0)
    {
        LOGERR("Fail to setup cookie. Result: %s", tls_error(result));

        return result;
    }

    mbedtls_ssl_conf_dtls_cookies(&servctx->conf, mbedtls_ssl_cookie_write, 
                                                  mbedtls_ssl_cookie_check,
                                                  &servctx->cookie_ctx);

    return 0;
}

static void *tls_accept(void *ctx)
{
    int result = 0;
    mbedtls_net_context client_fd;
    struct servctx_s *servctx = (struct servctx_s *) ctx;
    struct connctx_s *connctx = NULL;

    if(ctx == NULL)
    {
        LOGERR("Invalid argument");

        return NULL;
    }

    mbedtls_net_init(&client_fd);

    result = mbedtls_net_accept(&servctx->listen_fd, &client_fd, NULL, 0, NULL);
    if(result < 0)
    {
        LOGERR("Fail to accept. Result: %s", tls_error(result));

        return NULL;
    }

    LOGINF("New connection %d", client_fd.fd);

    connctx = malloc(sizeof(struct connctx_s));
    if(connctx == NULL)
    {
        LOGERR("Fail to allocate memory for connection context object");

        return NULL;
    }

    memcpy(&connctx->client_fd, &client_fd, sizeof(client_fd));

    mbedtls_ssl_init(&connctx->ssl);

    result = mbedtls_ssl_setup(&connctx->ssl, &servctx->conf);
    if(result < 0)
    {
        LOGERR("Fail to setup SSL");

        free(connctx);

        return NULL;
    }

    mbedtls_ssl_set_timer_cb(&connctx->ssl, &connctx->timer, mbedtls_timing_set_delay,
                                                             mbedtls_timing_get_delay);

    mbedtls_ssl_set_bio(&connctx->ssl, &connctx->client_fd, mbedtls_net_send,
                                                            mbedtls_net_recv,
                                                            mbedtls_net_recv_timeout);

    do
    {
        result = mbedtls_ssl_handshake(&connctx->ssl);
    }while(result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE);

    if(result < 0)
    {
        LOGERR("Fail to handshake. Result: %s", tls_error(result));

        free(connctx);

        return NULL;
    }

    return connctx;
}

static void tls_deinit(void *ctx)
{
    struct servctx_s *servctx = (struct servctx_s *) ctx;

    if(ctx == NULL)
    {
        LOGERR("Invalid argument");

        return;
    }

    mbedtls_net_free(&servctx->listen_fd);
    mbedtls_x509_crt_free(&servctx->srvcert);
    mbedtls_pk_free(&servctx->pkey);
    mbedtls_ssl_config_free(&servctx->conf);
    mbedtls_ssl_cookie_free(&servctx->cookie_ctx);
    mbedtls_ctr_drbg_free(&servctx->ctr_drbg);
    mbedtls_entropy_free(&servctx->entropy);

    free(servctx);
}

static int tls_recv(void *ctx, char *buf, size_t len)
{
    struct connctx_s *connctx = (struct connctx_s *) ctx;

    if(ctx == NULL || buf == NULL)
    {
        LOGERR("Invalid argument");

        return -EINVAL;
    }

    do
    {
        len = mbedtls_ssl_read(&connctx->ssl, (unsigned char *)buf, len);
    }while(len == MBEDTLS_ERR_SSL_WANT_READ || len == MBEDTLS_ERR_SSL_WANT_WRITE);

    return len;
}

static int tls_send(void *ctx, char *buf, size_t len)
{
    struct connctx_s *connctx = (struct connctx_s *) ctx;

    if(ctx == NULL || buf == NULL)
    {
        LOGERR("Invalid argument");

        return -EINVAL;
    }

    do
    {
        len = mbedtls_ssl_write(&connctx->ssl, (unsigned char *)buf, len);
    }while(len == MBEDTLS_ERR_SSL_WANT_READ || len == MBEDTLS_ERR_SSL_WANT_WRITE);

    return len;
}

static void tls_conn_close(void *ctx)
{
    int result = 0;
    struct connctx_s *connctx = (struct connctx_s *) ctx;

    if(ctx == NULL)
    {
        LOGERR("Invalid argument");

        return;
    }

    LOGINF("Connection %d close", connctx->client_fd.fd);

    /* No error checking, the connection might be closed already */
    do
    {
        result = mbedtls_ssl_close_notify(&connctx->ssl);
    }while(result == MBEDTLS_ERR_SSL_WANT_WRITE );

    mbedtls_net_free(&connctx->client_fd);
    mbedtls_ssl_free(&connctx->ssl);

    free(connctx);
}

const static struct conn_iface_s conn_iface =
{
    .recv  = tls_recv,
    .send  = tls_send,
    .close = tls_conn_close
};

static struct conn_iface_s *tls_conn_iface_get(void)
{
    return (struct conn_iface_s *)&conn_iface;
}

const static struct server_iface_s serv_iface =
{
    .init       = tls_init,
    .accept     = tls_accept,
    .deinit     = tls_deinit,
    .conn_iface = tls_conn_iface_get
};

struct server_iface_s *tls_serv_iface_get(void)
{
    return (struct server_iface_s *)&serv_iface;
}

void *tls_serv_ctx_alloc(void)
{
    struct servctx_s *servctx = NULL;

    servctx = malloc(sizeof(struct servctx_s));
    if(servctx == NULL)
    {
        LOGERR("Fail to allocate memory for server context");

        return NULL;
    }

    return servctx;
}
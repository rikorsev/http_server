#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <libgen.h>

#include "server.h"
#include "http.h"
#include "config.h"
#include "log.h"

#define MODULE_NAME "http"

enum resource_type_e
{
    RESOURCE_TYPE_TEXT_HTTP,
    RESOURCE_TYPE_TEXT_XML,
    RESOURCE_TYPE_TEXT_JSON,
    RESOURCE_TYPE_FILE,
    RESOURCE_TYPE_INVALID = -1
};

struct http_req_s
{
    char method[8];
    char path[CONFIG_MAX_PATH_SIZE];
    enum resource_type_e type;
    int keepalive;
};

struct http_resp_s
{
    char status[128];
    char header[128];
    FILE *file;
};

static int http_send_responce(void *connctx, struct http_resp_s *resp)
{
    char buf[CONFIG_OUTPUT_BUFF_LEN] = {0};
    int sendlen = 0;
    int readlen = 0;

    /* Check output arguments */
    if(resp == NULL)
    {
        LOGERR("Invalid argument");

        return -EINVAL;
    }

    /* Copy status to output buffer */
    strcpy(buf, resp->status);
    sendlen += strlen(resp->status);

    /* Copy headers to output bufer */
    strcpy(buf + sendlen, resp->header);
    sendlen += strlen(resp->header);

    /* Send status and header */
    sendlen = server_send(connctx, buf, sendlen);
    if(sendlen < 0)
    {
        LOGERR("Fail to send status and header Result %d", sendlen);

        return sendlen;
    }

    /* Send file if necessary */
    if(resp->file != NULL)
    {
        while((readlen = fread(buf, sizeof(char), CONFIG_OUTPUT_BUFF_LEN, resp->file)) != 0)
        {
            sendlen = server_send(connctx, buf, readlen);
            if(sendlen < 0)
            {
                LOGERR("Fail to send file. Result %d", sendlen);

                return sendlen;
            }
        }
    }

    return 0;
}

static int http_send_not_found(void *connctx)
{
    int result = 0;

    struct http_resp_s resp =
    {
        .status = "HTTP/1.1 404 Not Found\n\n",
        .header = "Not Found\n\n",
        .file = NULL
    };

    LOGINF("404: page not found");

    /* Send responce header */
    result = http_send_responce(connctx, &resp);
    if(result < 0)
    {
        LOGERR("Fail to send responce. Result: %d", result);

        return result;
    }

    return 0;
}

static int http_send_not_implemented(void *connctx)
{
    int result = 0;

    struct http_resp_s resp = 
    {
        .status = "HTTP/1.1 501 Not Implemented\n\n",
        .header = "Not Implemented\n\n",
        .file = NULL
    };

    LOGINF("501: not implemented");

    /* Send responce header */
    result = http_send_responce(connctx, &resp);
    if(result < 0)
    {
        LOGERR("Fail to send responce. Result: %d", result);

        return result;
    }

    return 0;
}

static int http_send_bad_request(void *connctx)
{
    int result = 0;

    struct http_resp_s resp = 
    {
        .status = "HTTP/1.1 400 Bad Request\n\n",
        .header = "Bad Request\n\n",
        .file = NULL
    };

    LOGINF("400: bad request");

    /* Send responce header */
    result = http_send_responce(connctx, &resp);
    if(result < 0)
    {
        LOGERR("Fail to send responce. Result: %d", result);

        return result;
    }

    return 0;
}

static enum resource_type_e http_resource_type_get(char *filepath)
{
    char *filebase = NULL;
    char *extension = NULL;
    enum resource_type_e type = RESOURCE_TYPE_INVALID;

    char *dupfilepath = strdup(filepath);

    filebase = strtok(dupfilepath, ".");
    if(filebase == NULL)
    {
        LOGERR("Fail to parse filebase");

        goto exit;
    }
    
    extension = strtok(NULL, ".");
    if(extension == NULL)
    {
        LOGERR("Fail to parse extension");

        goto exit;
    }

    if(strcmp(extension, "html") == 0)
    {
        type = RESOURCE_TYPE_TEXT_HTTP;

        goto exit;

    }

    if(strcmp(extension, "xml") == 0)
    {
        type = RESOURCE_TYPE_TEXT_XML;

        goto exit;

    }

    if(strcmp(extension, "json") == 0)
    {
        type = RESOURCE_TYPE_TEXT_JSON;

        goto exit;
    }

    /* By default type is file */
    type = RESOURCE_TYPE_FILE;

exit:

    free(dupfilepath);

    return type;
}

static int http_header_generate(struct http_req_s *req, struct http_resp_s *resp)
{
    size_t offset = 0;

    static const char text_html[] = "text/html";
    static const char text_xml[] = "text/xml";
    static const char text_json[] = "text/json";
    static const char text_header_format[] = "Content-type: %s\n";
    static const char file_header_format[] = "Content-Disposition: attachment; filename=%s\n";
    static const char keepalive[] = "Connection: keep-alive\n";
    
    if(req == NULL)
    {
        LOGERR("Invalid parametr");

        return -EINVAL;
    }

    switch(req->type)
    {
        case RESOURCE_TYPE_TEXT_HTTP:
            offset = sprintf(resp->header, text_header_format, text_html);
            break;
        
        case RESOURCE_TYPE_TEXT_XML:
            offset = sprintf(resp->header, text_header_format, text_xml);
            break;

        case RESOURCE_TYPE_TEXT_JSON:
            offset = sprintf(resp->header, text_header_format, text_json);
            break;

        default:
            offset = sprintf(resp->header, file_header_format, req->path);
    }

    /// @todo: check for buffer overflow

    /* Append keepalive */
    if(req->keepalive > 0)
    {
        strcpy(resp->header + offset, keepalive);
        offset += strlen(keepalive);
    }

    /* Append final /n */
    strcpy(resp->header + offset, "\n");

    return 0;
}

#if CONFIG_KEEPALIVE_ENABLE
static int http_keepalive_parse(char *buf, size_t len, struct http_req_s *req)
{
    if(buf == NULL || req == NULL)
    {
        return -EINVAL;
    }

    req->keepalive = 0;

    if(strstr(buf, "Connection: keep-alive") != NULL)
    {
        req->keepalive = 1;
    }

    return 0;
}
#endif

int http_request_parse(char *buf, size_t len, struct http_req_s *req)
{
    int result = 0;
    char *dupbuf = strdup(buf);
    char *token = NULL;

    if(dupbuf == NULL)
    {
        LOGERR("Fail to duplicate incoming buffer");

        return -ENOMEM;
    }

    /* Get method */
    token = strtok(dupbuf, " ");
    if(token == NULL)
    {
        LOGERR("Fail to parse method");

        result = -ENOMSG;

        goto exit;
    }

    strcpy(req->method, token);

    /* Get path */
    token = strtok(NULL, " ");
    if(token == NULL)
    {
        LOGERR("Fail to parse path");

        result = -ENOMSG;

        goto exit;
    }

    if(strlen(token) > CONFIG_MAX_PATH_SIZE)
    {
        LOGERR("Path is to big (%lu bytes)", strlen(token));

        result = -ENOMEM;

        goto exit;
    }

    strcpy(&req->path[1], token);

    /* Add . at the begining of path to make it relative */
    req->path[0] = '.';

    /* Replase / with index.html */
    if(strcmp(req->path, "./") == 0)
    {
        strcpy(req->path, "./index.html");
    }

    /* Check for ../ and ~/ sumbols in path */
    if(strstr(req->path, "../") != NULL || strstr(req->path, "~/") != NULL)
    {
        LOGERR("Path conatins ../ or ~/");

        result = -EINVAL;

        goto exit;
    }

    /* Get resource type */
    req->type = http_resource_type_get(basename(req->path));

    if(RESOURCE_TYPE_INVALID == req->type)
    {
        LOGERR("Invalid resource type");

        result = -EINVAL;

        goto exit;
    }

#if CONFIG_KEEPALIVE_ENABLE
    result = http_keepalive_parse(buf, len, req);
#endif

exit:
    free(dupbuf);

    return result;
}

int http_handler(void *connctx, char *buf, size_t len)
{
    int result = 0;
    struct http_req_s req = {0};
    struct http_resp_s resp = { .status = "HTTP/1.1 200 OK\n" };

    result = http_request_parse(buf, len, &req);
    if(result < 0)
    {
        LOGERR("Fail to parse request. Result: %d", result);

        http_send_bad_request(connctx);

        return result;
    }

    LOGINF("METHOD: %s", req.method);
    LOGINF("PATH: %s", req.path);
    LOGINF("KEEPALIFE: %d", req.keepalive);

    /* Our server supports only GET method, so if not,
        retuen 501 error then */
    if(strcmp(req.method, "GET") != 0)
    {
        LOGERR("%s not implemented\r\n", req.method);

        http_send_not_implemented(connctx);

        return -ENOMSG;
    }

    /* Open the resource file */
    resp.file = fopen(req.path, "r");
    if(resp.file == NULL)
    {
        LOGERR("Fail to open %s", req.path);

        /* send 404 */
        http_send_not_found(connctx);

        return -ENOENT;
    }

    /* Generate header */
    result = http_header_generate(&req, &resp);
    if(result < 0)
    {
        LOGERR("Fail to generate header. Result %d", result);

        fclose(resp.file);

        return -ENOMEM;
    }

    /* Send requested file */
    result = http_send_responce(connctx, &resp);
    if(result < 0)
    {
        LOGERR("Fail to send responce. Result %d", result);

        fclose(resp.file);

        return 0;
    }

    LOGINF("Request handled successfully");

#if CONFIG_KEEPALIVE_ENABLE
    return req.keepalive;
#else
    return 0;
#endif
}
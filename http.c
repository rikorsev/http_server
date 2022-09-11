#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <libgen.h>

#include "server.h"
#include "http.h"

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
    char path[128];
    enum resource_type_e type;
    int keepalive;
};

struct http_resp_s
{
    char status[128];
    char header[128];
    FILE *file;
};

static int http_send_responce(int conn, struct http_resp_s *resp)
{
    #define BUF_LEN 1024

    char buf[BUF_LEN];
    int sendlen = 0;
    int readlen = 0;

    /* Check output arguments */
    if(resp == NULL)
    {
        fprintf(stderr, "http: Invalid arguments\r\n");

        return -EINVAL;
    }

    /* Send status */
    sendlen = server_send(conn, resp->status, strlen(resp->status));
    if(sendlen < 0)
    {
        fprintf(stderr, "http: Fail to send responce. Result: %d\r\n", sendlen);

        return sendlen;
    }

    /* Send header */
    sendlen = server_send(conn, resp->header, strlen(resp->header));
    if(sendlen < 0)
    {
        fprintf(stderr, "http: Fail to send header. Result %d\r\n", sendlen);

        return sendlen;
    }

    /* Send file if necessary */
    if(resp->file != NULL)
    {
        while((readlen = fread(buf, sizeof(char), BUF_LEN, resp->file)) != 0)
        {
            sendlen = server_send(conn, buf, readlen);
            if(sendlen < 0)
            {
                fprintf(stderr, "http: Fail to send file. Result %d\r\n", sendlen);

                return sendlen;
            }
        }
    }

    return 0;
}

static int http_send_not_found(int conn)
{
    int result = 0;
    
    struct http_resp_s resp = 
    {
        .status = "HTTP/1.1 404 Not Found\n\n",
        .header = "Not Found\n\n",
        .file = NULL
    };

    printf("http: 404: page not found\r\n");

    /* Send responce header */
    result = http_send_responce(conn, &resp);
    if(result < 0)
    {
        fprintf(stderr, "http: Fail to send responce. Result: %d\r\n", result);

        return result;
    }

    return 0;
}

static int http_send_not_implemented(int conn)
{
    int result = 0;
    
    struct http_resp_s resp = 
    {
        .status = "HTTP/1.1 501 Not Implemented\n\n",
        .header = "Not Implemented\n\n",
        .file = NULL
    };

    printf("http: 501: not implemented\r\n");

    /* Send responce header */
    result = http_send_responce(conn, &resp);
    if(result < 0)
    {
        fprintf(stderr, "http: Fail to send responce. Result: %d\r\n", result);

        return result;
    }

    return 0;
}

static int http_send_bad_request(int conn)
{
    int result = 0;
    
    struct http_resp_s resp = 
    {
        .status = "HTTP/1.1 400 Bad Request\n\n",
        .header = "Bad Request\n\n",
        .file = NULL
    };

    printf("http: 400: bad request\r\n");

    /* Send responce header */
    result = http_send_responce(conn, &resp);
    if(result < 0)
    {
        fprintf(stderr, "http: Fail to send responce. Result: %d\r\n", result);

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
        fprintf(stderr, "http: Fail to parse filebase\r\n");

        goto exit;
    }
    
    extension = strtok(NULL, ".");
    if(extension == NULL)
    {
        fprintf(stderr, "http: Fail to parse extension\r\n");

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

    /* Bu default type is file */
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
        fprintf(stderr, "http: Invalid parametr\r\n");

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

int http_request_parse(char *buf, size_t len, struct http_req_s *req)
{
    int result = 0;
    char *dupbuf = strdup(buf);
    char *token = NULL;

    if(dupbuf == NULL)
    {
        fprintf(stderr, "http: Fail to duplicate incoming buffer\r\n");

        return -ENOMEM;
    }

    /* Get method */
    token = strtok(dupbuf, " ");
    if(token == NULL)
    {
        fprintf(stderr, "http: Fail to parse method\r\n");

        result = -ENOMSG;

        goto exit;
    }

    strcpy(req->method, token);

    /* Get path */
    token = strtok(NULL, " ");
    if(token == NULL)
    {
        fprintf(stderr, "http: Fail to parse path\r\n");

        result = -ENOMSG;

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
        fprintf(stderr, "http: Path conatins ../ or ~/\r\n");

        result = -EINVAL;

        goto exit;
    }

    /* Get resource type */
    req->type = http_resource_type_get(basename(req->path));

    if(RESOURCE_TYPE_INVALID == req->type)
    {
        fprintf(stderr, "http: Invalid resource type\r\n");
        
        result = -EINVAL;

        goto exit;
    }

    result = http_keepalive_parse(buf, len, req);

exit:
    free(dupbuf);

    return result;
}

int http_handler(int conn, char *buf, size_t len)
{
    int result = 0;
    struct http_req_s req = {0};
    struct http_resp_s resp = { .status = "HTTP/1.1 200 OK\n" };

    printf("http: Request handling started(conn %d)\r\n", conn);

    result = http_request_parse(buf, len, &req);
    if(result < 0)
    {
        fprintf(stderr, "http: Fail to parse request. Result: %d\r\n", result);

        http_send_bad_request(conn);

        return result;
    }

    printf("METHOD: %s\r\n", req.method);
    printf("PATH: %s\r\n", req.path);
    printf("KEEPALIFE: %d\r\n", req.keepalive);

    /* Our server supports only GET method, so if not,
        retuen 501 error then */
    if(strcmp(req.method, "GET") != 0)
    {
        fprintf(stderr, "http: %s not implemented\r\n", req.method);

        http_send_not_implemented(conn);

        return -ENOMSG;
    }

    /* Open the resource file */
    resp.file = fopen(req.path, "r");
    if(resp.file == NULL)
    {
        fprintf(stderr, "http: Fail to open %s\r\n", req.path);

        /* send 404 */
        http_send_not_found(conn);

        return -ENOENT;
    }

    /* Generate header */
    result = http_header_generate(&req, &resp);
    if(result < 0)
    {
        fprintf(stderr, "http: Fail to generate header. Result %d\r\n", result);

        fclose(resp.file);

        return -ENOMEM;
    }

    /* Send requested file */
    result = http_send_responce(conn, &resp);
    if(result < 0)
    {
        fprintf(stderr, "http: Fail to send responce. Result %d\r\n", result);

        fclose(resp.file);

        return 0;
    }

    printf("http: Request handled successfully\r\n");

    return req.keepalive;
}
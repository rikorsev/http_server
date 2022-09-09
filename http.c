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
    int keepalive;
};

static int http_send_ok(int conn)
{
    char respbuf[] = "HTTP/1.1 200 OK\n";
    int sendlen = 0;

    /* Send responce header */
    sendlen = server_send(conn, respbuf, strlen(respbuf));
    if(sendlen < 0)
    {
        fprintf(stderr, "http: Fail to send responce. Result: %d\r\n", sendlen);

        return sendlen;
    }

    return 0;
}

static int http_send_not_found(int conn)
{
    char respbuf[] = "HTTP/1.1 404 Not Found\n\nNot Found";
    int sendlen = 0;
    
    printf("http: 404: page not found\r\n");

    /* Send responce header */
    sendlen = server_send(conn, respbuf, sizeof(respbuf));
    if(sendlen < 0)
    {
        fprintf(stderr, "http: Fail to send responce. Result: %d\r\n", sendlen);

        return sendlen;
    }

    return 0;
}

static int http_send_not_implemented(int conn)
{
    char respbuf[] = "HTTP/1.1 501 Not Implemented\n\nNot Implemented";
    int sendlen = 0;
    
    printf("http: 501: not implemented\r\n");

    /* Send responce header */
    sendlen = server_send(conn, respbuf, sizeof(respbuf));
    if(sendlen < 0)
    {
        fprintf(stderr, "http: Fail to send responce. Result: %d\r\n", sendlen);

        return sendlen;
    }

    return 0;
}

static int http_send_bad_request(int conn)
{
    char respbuf[] = "HTTP/1.1 400 Bad Request\n\nBad Request";
    int sendlen = 0;
    
    printf("http: 400: bad request\r\n");

    /* Send responce header */
    sendlen = server_send(conn, respbuf, sizeof(respbuf));
    if(sendlen < 0)
    {
        fprintf(stderr, "http: Fail to send responce. Result %d\r\n", sendlen);

        return sendlen;
    }

    return 0;
}

static int http_send_file(int conn, char *header, FILE *file)
{
    #define BUF_LEN 1024

    char buf[BUF_LEN];
    int sendlen = 0;
    int readlen = 0;
    int result = 0;

    /* Check output arguments */
    if(header == NULL || file == NULL)
    {
        fprintf(stderr, "http: Invalid arguments\r\n");

        return -1;
    }

    /* Send OK status */
    result = http_send_ok(conn);
    if(result < 0)
    {
        fprintf(stderr, "http: Fail to send ok responce. Result: %d\r\n", result);

        return result;
    }

    /* Send header */
    sendlen = server_send(conn, header, strlen(header));
    if(sendlen < 0)
    {
        fprintf(stderr, "http: Fail to send header. Result %d\r\n", sendlen);

        return sendlen;
    }

    /* Send index file */
    while((readlen = fread(buf, sizeof(char), BUF_LEN, file)) != 0)
    {
        sendlen = server_send(conn, buf, readlen);
        if(sendlen < 0)
        {
            fprintf(stderr, "http: Fail to send index. Result %d\r\n", sendlen);

            return sendlen;
        }
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

static char *http_header_generate(char *filename)
{
    char *header = NULL;

    static const char text_html[] = "text/html";
    static const char text_xml[] = "text/xml";
    static const char text_json[] = "text/json";
    static const char text_header_format[] = "Content-type: %s\n\n";
    static const char file_header_format[] = "Content-Disposition: attachment; filename=%s\n\n";

    enum resource_type_e type = http_resource_type_get(filename);

    if(filename == NULL)
    {
        fprintf(stderr, "http: invalid parametr\r\n");

        return NULL;
    }

    /* Invalid resource type. Return NULL */
    if(type == RESOURCE_TYPE_INVALID)
    {
        return NULL;
    }

    /* Brutal :) */
    /* Allocate memory for header */
    header = malloc(128);
    if(header == NULL)
    {
        fprintf(stderr, "http: Fail to allocate memory for header\r\n");

        return NULL;
    }

    switch(type)
    {
        case RESOURCE_TYPE_TEXT_HTTP:
            sprintf(header, text_header_format, text_html);
            break;
        
        case RESOURCE_TYPE_TEXT_XML:
            sprintf(header, text_header_format, text_xml);
            break;

        case RESOURCE_TYPE_TEXT_JSON:
            sprintf(header, text_header_format, text_json);
            break;

        default:
            sprintf(header, file_header_format, filename);
    }

    return header;
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

    printf("BUF: %s\r\n", buf);

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

    result = http_keepalive_parse(buf, len, req);

exit:
    free(dupbuf);

    return result;
}

int http_handler(int conn, char *buf, size_t len)
{
    char *header = NULL;

    struct http_req_s req = {0};

    FILE *file = NULL;
    int result = 0;

    result = http_request_parse(buf, len, &req);
    if(result < 0)
    {
        fprintf(stderr, "http: fail to parse request. Result: %d", result);

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
    file = fopen(req.path, "r");
    if (file == NULL)
    {
        fprintf(stderr, "http: Fail to open %s\r\n", req.path);

        /* send 404 */
        http_send_not_found(conn);

        return -ENOENT;
    }

    /* Generate header */
    header = http_header_generate(basename(req.path));
    if(header == NULL)
    {
        fprintf(stderr, "http: Fail to generate header %s\r\n", req.path);

        fclose(file);

        return -ENOMEM;
    }

    /* Send requested file */
    result = http_send_file(conn, header, file);
    if(result < 0)
    {
        fprintf(stderr, "http: Fail to send %s. Result\r\n", req.path);

        fclose(file);
        free(header);

        return 0;
    }

    printf("http: Request handled successfully\r\n");

    return req.keepalive;
}
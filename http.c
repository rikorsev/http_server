#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "server.h"
#include "http.h"

struct http_table_s *http_table_create(int resources_num)
{
    struct http_table_s *table = NULL;

    /* Allocate memory for http table */
    table = malloc(sizeof(struct http_table_s));
    if(table == NULL)
    {
        fprintf(stderr, "http: fail to allocate mamory for http table\r\n");

        return NULL;
    }

    /* Allocate memory for http resources */
    table->res = malloc(resources_num * sizeof(struct http_resource_s));
    if(table->res == NULL)
    {
        fprintf(stderr, "http: fail to allocate mamory for http resources\r\n");

        free(table);

        return NULL;
    }

    /* Set number of resources */
    table->num = resources_num;

    /* Set resource content to zero */
    memset(table->res, 0, resources_num * sizeof(struct http_resource_s));

    return table;
}

void http_table_free(struct http_table_s *table)
{
    free(table->res);
    free(table);
}

int http_table_register_resource(struct http_table_s *table, char *path, http_resource_handler_f handler)
{
    /* Ceck if path or handler is NULL */
    if(path == NULL || handler == NULL)
    {
        fprintf(stderr, "http: path or handler NULL\r\n");

        return -1;
    }

    /* Check for duplications */
    for(int i = 0; i < table->num; i++)
    {
        if(table->res[i].path == NULL)
        {
            continue;
        }

        if(strcmp(table->res[i].path, path) == 0)
        {
            fprintf(stderr, "http: resources duplication\r\n");

            return -1;
        }
    }

    /* Add resource at free slot */
    int i = 0;
    for(; i < table->num; i++)
    {
        if(table->res[i].handler == NULL)
        {
            table->res[i].path = path;
            table->res[i].handler = handler;

            break;
        }
    }

    /* if i == to table num it means that there is no free space for new resource */
    if(i == table->num)
    {
        fprintf(stderr, "http: no space available for new resource\r\n");

        return -1;
    }

    return 0;
}

static enum method_e str_to_method(char *methodstr)
{
    if(strcmp(methodstr, "GET") == 0)
    {
        return METHOD_GET;
    }
    
    return METHOD_INVALID;
}

static void http_send_not_found(int conn)
{
    char respbuf[] = "HTTP/1.1 404 Not Found\n\nNot Found";
    int sendlen = 0;
    
    printf("http: 404: page not found\r\n");

    /* Send responce header */
    sendlen = server_send(conn, respbuf, sizeof(respbuf));
    if(sendlen < 0)
    {
        fprintf(stderr, "main: Fail to send responce\r\n");
    }
}

void http_handler(int conn, char *buf, size_t len, void *meta, size_t metalen)
{
    char *methodstr = NULL;
    char *path = NULL;

    struct http_table_s *table = (struct http_table_s *) meta;

    printf("%s", buf);

    /* Parse Header */
    methodstr = strtok(buf, " ");
    path = strtok(NULL, " ");

    printf("METHOD: %s\r\n", methodstr);
    printf("RESOURCE: %s\r\n", path);

    /* Searching for requested resource */
    int i = 0;
    for(; i < table->num; i++)
    {

        if(table->res[i].path == NULL)
        {
            continue;
        }

        if(strcmp(table->res[i].path, path) == 0)
        {
            table->res[i].handler(conn, str_to_method(methodstr), buf, len);

            break;
        }
    }

    /* If required resourse were not found */
    if(i == table->num)
    {
        /* send 404 */
        http_send_not_found(conn);
    }
}
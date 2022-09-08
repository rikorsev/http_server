#ifndef HTTP_H_
#define HTTP_H_

enum method_e
{
    METHOD_GET,
    METHOD_INVALID
};

typedef void (*http_resource_handler_f)(int conn, enum method_e method, char *buf, size_t len);

struct http_resource_s
{
    char *path;
    http_resource_handler_f handler;
};

struct http_table_s
{
    int num;
    struct http_resource_s *res;
};

struct http_table_s *http_table_create(int resources_num);
void http_table_free(struct http_table_s *table);
int http_table_register_resource(struct http_table_s *table, char *path, http_resource_handler_f handler);
void http_handler(int conn, char *buf, size_t len, void *meta, size_t metalen);

#endif
#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <time.h>

#define LOGINF(msg, ...) fprintf(stdout, "INF: [%08ld] ", time(NULL));\
                         fprintf(stdout, "%s: ", MODULE_NAME);\
                         fprintf(stdout, msg, ##__VA_ARGS__);\
                         fprintf(stdout, "\r\n")

#define LOGERR(msg, ...) fprintf(stderr, "ERR: [%08ld] ", time(NULL));\
                         fprintf(stderr, "%s: ", MODULE_NAME);\
                         fprintf(stderr, msg, ##__VA_ARGS__);\
                         fprintf(stderr, "\r\n")

#endif
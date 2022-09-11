/**
 * @file log.h
 * @brief Provide access to loggigh macroses
 * 
 **/

#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <time.h>

/**
 * @brief Logout INFO message
 * 
 * @param msg[in] - formatted message to output
 * @param ...[in] - additional arguments
 * 
 **/
#define LOGINF(msg, ...) fprintf(stdout, "INF: [%08ld] ", time(NULL));\
                         fprintf(stdout, "%s: ", MODULE_NAME);\
                         fprintf(stdout, msg, ##__VA_ARGS__);\
                         fprintf(stdout, "\r\n")

/**
 * @brief Logout ERROR message
 * 
 * @param msg[in] - formatted message to output
 * @param ...[in] - additional arguments
 * 
 **/
#define LOGERR(msg, ...) fprintf(stderr, "ERR: [%08ld] ", time(NULL));\
                         fprintf(stderr, "%s: ", MODULE_NAME);\
                         fprintf(stderr, msg, ##__VA_ARGS__);\
                         fprintf(stderr, "\r\n")

#endif
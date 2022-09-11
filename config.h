/**
 * @file config.h
 * @brief Provide access the solution configuration options
 * 
 **/

#ifndef CONFIG_H_
#define CONFIG_H_

/** Enable support of keep-alive feature */
#define CONFIG_KEEPALIVE_ENABLE 0

/** Define timeout for keep-alive in seconds */
#define CONFIG_KEEPALIVE_TIMEOUT_SEC 15

/** Define size of buffer for input (from client to server) data in bytes */
#define CONFIG_INPUT_BUFF_LEN 1024

/** Define size of buffer for output (rom server to client) data in bytes */
#define CONFIG_OUTPUT_BUFF_LEN 1024

/** Define maxinum path size in HTTP request */
#define CONFIG_MAX_PATH_SIZE 128

#endif
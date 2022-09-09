#ifndef HTTP_H_
#define HTTP_H_

/**
 * @brief HTTP data handler
 * 
 * The function parses incoming request stored in buffer 
 * and handle it according to the requested data.
 * 
 * @param conn[in] - connection descriptor
 * @param buf[in] - bufer that contains incoming data
 * @param len[in] - lenght of incoming data
 * 
 * @retval 1 if connection shall remains open, 0 otherwise
 **/
int http_handler(int conn, char *buf, size_t len);

#endif
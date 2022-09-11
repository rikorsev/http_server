#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <unistd.h>

#include "server.h"
#include "http.h"
#include "log.h"

#define MODULE_NAME "main"

const char *argp_program_version = "0.1.0";
const char *argp_program_bug_address = "<rikorsev@gmail.com>";
 
/* Program documentation. */
static char doc[] = "Simple HTTP server writen on C";

/* A description of the arguments we accept. */

/* The options we understand. */
static struct argp_option options[] = {
  {"root", 'r', "root", 0, "Rood directory for resources" },
  {"addr", 'a', "addr", 0, "IP address of the server"},
  {"port", 'p', "port", 0, "TCP port to access server" },
  { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments
{
    char *root;
    char *addr;
    int port;
};

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    /* Get the input argument from argp_parse, which we
        know is a pointer to our arguments structure. */
    struct arguments *arguments = state->input;

    switch (key)
    {
        case 'r':
            arguments->root = arg;
            break;
        
        case 'a':
            arguments->addr = arg;
            break;

        case 'p':
            arguments->port = atoi(arg);
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

/* argp parser. */
static struct argp argp = { options, parse_opt, NULL, doc };

static int start_server(char *addr, int port, server_listen_handler_f handler)
{
    int sockfd = 0;
    sockfd = server_create(addr, port);
    if(sockfd < 0)
    {
        LOGERR("Fail to create new server. Result %d", sockfd);

        return sockfd;
    }

    if(server_listen(sockfd, handler) < 0)
    {
        LOGERR("Fail to listen");

        server_close(sockfd);

        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    struct arguments arguments;

    /* Default values. */
    arguments.root = ".";
    arguments.addr = "127.0.0.1";
    arguments.port = 80;

    /* Parse our arguments; every option seen by parse_opt will
        be reflected in arguments. */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    LOGINF("Root: %s", arguments.root);
    LOGINF("Address: %s", arguments.addr);
    LOGINF("Port: %d", arguments.port);

    /* Change directory to specefied */
    if(chdir(arguments.root) < 0)
    {
        LOGERR("Fail to change directory. Result: %s", strerror(errno));

        return -errno;
    }

    /* Start server */
    if(start_server(arguments.addr, arguments.port, http_handler) < 0)
    {
        LOGERR("Fail to start server");

        return -1;
    }

    return 0;
}

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <unistd.h>
#include <stdbool.h>

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
  {"root",   'r', "root", 0, "Rood directory for resources" },
  {"addr",   'a', "addr", 0, "IP address of the server"},
  {"port",   'p', "port", 0, "TCP port to access server" },
  {"secure", 's', 0, 0, "Create secure HTTPS connection"},
  { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments
{
    char *root;
    char *addr;
    int port;
    bool secure;
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

        case 's':
            arguments->secure = true;
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

/* argp parser. */
static struct argp argp = { options, parse_opt, NULL, doc };

static int start_server(char *addr, int port, bool secure, server_listen_handler_f handler)
{
    struct server_s *server = NULL;
    int result = 0;

    server = server_create(secure);
    if(server == NULL)
    {
        LOGERR("Fail to create new server");

        return -1;
    }

    result = server_init(server, addr, port);
    if(result < 0)
    {
        LOGERR("Fail to init server. Result %d", result);

        goto exit;
    }

    result = server_listen(server, handler);
    if(result < 0)
    {
        LOGERR("Fail to listen. Result %d", result);

        goto exit;
    }

exit:
    return server_close(server);

    free(server);

    return result;
}

int main(int argc, char **argv)
{
    struct arguments arguments;

    /* Default values. */
    arguments.root = ".";
    arguments.addr = "127.0.0.1";
    arguments.port = 80;
    arguments.secure = false;

    /* Parse our arguments; every option seen by parse_opt will
        be reflected in arguments. */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    LOGINF("Root: %s", arguments.root);
    LOGINF("Address: %s", arguments.addr);
    LOGINF("Port: %d", arguments.port);
    LOGINF("Secure: %s", arguments.secure ? "yes": "no");

    /* Change directory to specefied */
    if(chdir(arguments.root) < 0)
    {
        LOGERR("Fail to change directory. Result: %s", strerror(errno));

        return -errno;
    }

    /* Start server */
    if(start_server(arguments.addr, arguments.port, arguments.secure, http_handler) < 0)
    {
        LOGERR("Fail to start server");

        return -1;
    }

    return 0;
}

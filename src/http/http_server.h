#ifndef CEASY_HTTP_SERVER_H
#define CEASY_HTTP_SERVER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int socket_fd;
    uint16_t port;
    char error[256];
} HttpServer;

/* Starts TCP listener. Port 0 selects an available port. */
bool http_server_start(HttpServer *server, uint16_t port);
/* Blocks until one TCP client connects; caller owns returned fd. */
int http_server_accept(HttpServer *server);
void http_server_close(HttpServer *server);

uint16_t http_server_port(const HttpServer *server);
const char *http_server_error(const HttpServer *server);

#endif

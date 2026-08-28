#include "http_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define HTTP_SERVER_BACKLOG 16

static void http_server_set_error(HttpServer *server, const char *message)
{
    snprintf(server->error, sizeof(server->error), "%s", message);
}

bool http_server_start(HttpServer *server, uint16_t port)
{
    struct sockaddr_in address;
    socklen_t address_length = sizeof(address);
    int socket_fd;

    if (server == NULL) {
        return false;
    }

    memset(server, 0, sizeof(*server));
    server->socket_fd = -1;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        http_server_set_error(server, strerror(errno));
        return false;
    }

    int reuse_address = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        http_server_set_error(server, strerror(errno));
        close(socket_fd);
        return false;
    }

    if (listen(socket_fd, HTTP_SERVER_BACKLOG) < 0) {
        http_server_set_error(server, strerror(errno));
        close(socket_fd);
        return false;
    }

    if (getsockname(socket_fd, (struct sockaddr *)&address, &address_length) < 0) {
        http_server_set_error(server, strerror(errno));
        close(socket_fd);
        return false;
    }

    server->socket_fd = socket_fd;
    server->port = ntohs(address.sin_port);
    return true;
}

int http_server_accept(HttpServer *server)
{
    int client_fd;

    if (server == NULL || server->socket_fd < 0) {
        return -1;
    }

    client_fd = accept(server->socket_fd, NULL, NULL);
    if (client_fd < 0) {
        http_server_set_error(server, strerror(errno));
    }
    return client_fd;
}

void http_server_close(HttpServer *server)
{
    if (server == NULL || server->socket_fd < 0) {
        return;
    }

    close(server->socket_fd);
    server->socket_fd = -1;
}

uint16_t http_server_port(const HttpServer *server)
{
    return server == NULL ? 0 : server->port;
}

const char *http_server_error(const HttpServer *server)
{
    return server == NULL ? "invalid HTTP server" : server->error;
}

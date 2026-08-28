#include "ceasy.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static bool ceasy_read_request(int client_fd, char *buffer, size_t buffer_size)
{
    size_t bytes_read = 0;

    while (bytes_read + 1 < buffer_size) {
        ssize_t result = recv(client_fd, buffer + bytes_read, buffer_size - bytes_read - 1, 0);

        if (result <= 0) {
            return false;
        }

        bytes_read += (size_t)result;
        buffer[bytes_read] = '\0';
        if (strstr(buffer, "\r\n\r\n") != NULL) {
            return true;
        }
    }

    return false;
}

static void ceasy_handle_client(int client_fd)
{
    static const char response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello";
    static const char method_not_allowed[] =
        "HTTP/1.1 405 Method Not Allowed\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";
    char request[4096];
    const char *result;

    if (!ceasy_read_request(client_fd, request, sizeof(request))) {
        close(client_fd);
        return;
    }

    result = strncmp(request, "GET ", 4) == 0 ? response : method_not_allowed;
    send(client_fd, result, strlen(result), 0);
    close(client_fd);
}

void ceasy_run(int argc, char** argv)
{
    HttpServer server;

    (void)argc;
    (void)argv;

    if (!http_server_start(&server, 3000)) {
        fprintf(stderr, "could not start HTTP server: %s\n", http_server_error(&server));
        return;
    }

    while (true) {
        int client_fd = http_server_accept(&server);

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "could not accept HTTP client: %s\n", http_server_error(&server));
            break;
        }

        ceasy_handle_client(client_fd);
    }

    http_server_close(&server);
}

#include "http/http_server.h"

#include <arpa/inet.h>
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
    HttpServer server;
    struct sockaddr_in address;
    int client_fd;
    int accepted_fd;

    assert(http_server_start(&server, 0));
    assert(http_server_port(&server) != 0);

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(client_fd >= 0);
    address.sin_family = AF_INET;
    address.sin_port = htons(http_server_port(&server));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(connect(client_fd, (struct sockaddr *)&address, sizeof(address)) ==
           0);
    accepted_fd = http_server_accept(&server);
    assert(accepted_fd >= 0);

    close(accepted_fd);
    close(client_fd);
    http_server_close(&server);

    return 0;
}

#include "ceasy/ceasy.h"

#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void read_response(int fd, char *buffer, size_t capacity) {
    ssize_t length = recv(fd, buffer, capacity - 1, 0);

    assert(length > 0);
    buffer[length] = '\0';
}

int main(void) {
    int sockets[2];
    Context context = {0};
    char response[512];

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    context.client_fd = sockets[0];
    assert(context_send_text(&context, sv("200 OK"), sv("hello")));
    read_response(sockets[1], response, sizeof(response));
    assert(strstr(response, "Content-Type: text/plain; charset=utf-8") != NULL);
    assert(strstr(response, "Content-Length: 5") != NULL);
    assert(strstr(response, "\r\n\r\nhello") != NULL);
    close(sockets[0]);
    close(sockets[1]);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    context = (Context){.client_fd = sockets[0]};
    assert(context_redirect(&context, sv("/posts")));
    read_response(sockets[1], response, sizeof(response));
    assert(strstr(response, "303 See Other") != NULL);
    assert(strstr(response, "Location: /posts") != NULL);
    assert(strstr(response, "Content-Length: 0") != NULL);
    close(sockets[0]);
    close(sockets[1]);
    return 0;
}

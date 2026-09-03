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
    const unsigned char binary[] = {0x00, 0xff, 0x00, 0x7f};

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    context.client_fd = sockets[0];
    assert(context_send_text(&context, sv("200 OK"), sv("hello")));
    read_response(sockets[1], response, sizeof(response));
    assert(strstr(response, "Content-Type: text/plain; charset=utf-8") != NULL);
    assert(strstr(response, "Content-Length: 5") != NULL);
    assert(strstr(response, "\r\n\r\nhello") != NULL);
    close(sockets[0]);
    close(sockets[1]);

    Arena arena;
    assert(arena_init(&arena, 4096));
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    context = (Context){.client_fd = sockets[0], .arena = &arena};
    assert(context_set_header(&context, sv("X-Test"), sv("one")));
    assert(context_add_header(&context, sv("Set-Cookie"), sv("a=1")));
    assert(context_add_header(&context, sv("Set-Cookie"), sv("b=2")));
    assert(context_send_text(&context, sv("201 Created"), sv("ok")));
    read_response(sockets[1], response, sizeof(response));
    assert(strstr(response, "X-Test: one") != NULL);
    assert(strstr(response, "Set-Cookie: a=1") != NULL);
    assert(strstr(response, "Set-Cookie: b=2") != NULL);
    assert(strstr(response, "X-Content-Type-Options: nosniff") != NULL);
    close(sockets[0]);
    close(sockets[1]);
    arena_destroy(&arena);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    context = (Context){.client_fd = sockets[0]};
    assert(context_redirect(&context, sv("/posts")));
    read_response(sockets[1], response, sizeof(response));
    assert(strstr(response, "303 See Other") != NULL);
    assert(strstr(response, "Location: /posts") != NULL);
    assert(strstr(response, "Content-Length: 0") != NULL);
    close(sockets[0]);
    close(sockets[1]);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    context = (Context){.client_fd = sockets[0]};
    assert(context_send_bytes(&context, sv("200 OK"), sv("image/png"), binary,
                              sizeof(binary)));
    ssize_t response_length = recv(sockets[1], response, sizeof(response), 0);
    assert(response_length > 0);
    char *body = strstr(response, "\r\n\r\n");
    assert(body != NULL);
    body += 4;
    assert((size_t)(response + response_length - body) == sizeof(binary));
    assert(memcmp(body, binary, sizeof(binary)) == 0);
    close(sockets[0]);
    close(sockets[1]);
    return 0;
}

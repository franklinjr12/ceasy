#include "ceasy/ceasy.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static bool ceasy_send_all(int client_fd, StringView data) {
    size_t sent = 0;

    while (sent < data.length) {
        ssize_t result =
            send(client_fd, data.data + sent, data.length - sent, 0);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (result == 0) {
            return false;
        }
        sent += (size_t)result;
    }
    return true;
}

bool context_send_response(Context *context, StringView status,
                           StringView content_type, StringView body) {
    String header;
    bool sent;

    if (context == NULL || context->response_sent || context->client_fd < 0 ||
        status.data == NULL || content_type.data == NULL ||
        (body.length > 0 && body.data == NULL)) {
        return false;
    }
    header = context->arena != NULL ? string_new_in(context->arena)
                                    : string_new_heap();
    if (!string_append(&header, sv("HTTP/1.1 ")) ||
        !string_append(&header, status) ||
        !string_append(&header, sv("\r\nContent-Type: ")) ||
        !string_append(&header, content_type) ||
        !string_append_format(&header,
                              "\r\nContent-Length: %zu\r\n"
                              "Connection: close\r\n\r\n",
                              body.length)) {
        string_destroy(&header);
        return false;
    }
    sent = ceasy_send_all(context->client_fd, string_as_view(&header)) &&
           ceasy_send_all(context->client_fd, body);
    string_destroy(&header);
    if (sent) {
        context->response_sent = true;
    }
    return sent;
}

bool context_send_text(Context *context, StringView status, StringView body) {
    return context_send_response(context, status,
                                 sv("text/plain; charset=utf-8"), body);
}

bool context_send_html(Context *context, StringView status, StringView body) {
    return context_send_response(context, status,
                                 sv("text/html; charset=utf-8"), body);
}

bool context_redirect(Context *context, StringView location) {
    String header;
    bool sent;

    if (context == NULL || context->response_sent || context->client_fd < 0 ||
        location.data == NULL) {
        return false;
    }
    header = context->arena != NULL ? string_new_in(context->arena)
                                    : string_new_heap();
    if (!string_append(&header, sv("HTTP/1.1 303 See Other\r\nLocation: ")) ||
        !string_append(&header, location) ||
        !string_append(&header, sv("\r\nContent-Length: 0\r\n"
                                   "Connection: close\r\n\r\n"))) {
        string_destroy(&header);
        return false;
    }
    sent = ceasy_send_all(context->client_fd, string_as_view(&header));
    string_destroy(&header);
    if (sent) {
        context->response_sent = true;
    }
    return sent;
}

static RequestParseResult ceasy_read_request(Context *context) {
    size_t capacity = CEASY_MAX_HEADER_BYTES + CEASY_MAX_REQUEST_BODY;
    char *buffer = arena_alloc(context->arena, capacity);
    size_t received = 0;

    if (buffer == NULL) {
        return REQUEST_PARSE_BAD_REQUEST;
    }
    while (received < capacity) {
        ssize_t result =
            recv(context->client_fd, buffer + received, capacity - received, 0);
        RequestParseResult parsed;

        if (result <= 0) {
            return REQUEST_PARSE_BAD_REQUEST;
        }
        received += (size_t)result;
        parsed =
            request_parse(&context->request,
                          (StringView){.data = buffer, .length = received});
        if (parsed != REQUEST_PARSE_INCOMPLETE) {
            return parsed;
        }
    }
    return REQUEST_PARSE_PAYLOAD_TOO_LARGE;
}

static const char *ceasy_method_name(HttpMethod method) {
    switch (method) {
    case HTTP_METHOD_GET:
        return "GET";
    case HTTP_METHOD_POST:
        return "POST";
    case HTTP_METHOD_PATCH:
        return "PATCH";
    case HTTP_METHOD_PUT:
        return "PUT";
    case HTTP_METHOD_DELETE:
        return "DELETE";
    default:
        return "";
    }
}

static void ceasy_handle_client(int client_fd, Router *router) {
    Arena arena;
    Database database;
    Context context;
    RequestParseResult parsed;
    RouterResult dispatch_result;
    String path;

    memset(&context, 0, sizeof(context));
    context.client_fd = client_fd;
    if (!arena_init(&arena, 64 * 1024)) {
        close(client_fd);
        return;
    }
    context.arena = &arena;
    if (!database_open(&database, "db/development.sqlite3")) {
        context_send_text(&context, sv("500 Internal Server Error"),
                          sv("database error\n"));
        arena_destroy(&arena);
        close(client_fd);
        return;
    }
    context.database = &database;
    parsed = ceasy_read_request(&context);
    if (parsed != REQUEST_PARSE_OK) {
        if (parsed == REQUEST_PARSE_PAYLOAD_TOO_LARGE) {
            context_send_text(&context, sv("413 Payload Too Large"),
                              sv("payload too large\n"));
        } else {
            context_send_text(&context, sv("400 Bad Request"),
                              sv("malformed request\n"));
        }
        database_close(&database);
        arena_destroy(&arena);
        close(client_fd);
        return;
    }

    if (context.request.method == HTTP_METHOD_POST &&
        context.request.content_type.length >=
            sizeof("application/x-www-form-urlencoded") - 1 &&
        stringv_equal_ignore_case(
            stringv_slice(context.request.content_type, 0,
                          sizeof("application/x-www-form-urlencoded") - 1),
            sv("application/x-www-form-urlencoded"))) {
        if (!context_parse_form(&context)) {
            context_send_text(&context, sv("400 Bad Request"),
                              sv("malformed form\n"));
            database_close(&database);
            arena_destroy(&arena);
            close(client_fd);
            return;
        }
        StringView override = context_form(&context, sv("_method"));
        if (stringv_equal_ignore_case(override, sv("PATCH"))) {
            context.request.method = HTTP_METHOD_PATCH;
        } else if (stringv_equal_ignore_case(override, sv("DELETE"))) {
            context.request.method = HTTP_METHOD_DELETE;
        }
    }

    path = string_from_in(&arena, context.request.path);
    if (path.data == NULL) {
        context_send_text(&context, sv("500 Internal Server Error"),
                          sv("could not prepare path\n"));
        database_close(&database);
        arena_destroy(&arena);
        close(client_fd);
        return;
    }
    dispatch_result = router_dispatch(
        router, ceasy_method_name(context.request.method), path.data, &context);
    if (dispatch_result == ROUTER_NOT_FOUND) {
        context_send_text(&context, sv("404 Not Found"), sv("not found\n"));
    } else if (dispatch_result == ROUTER_METHOD_NOT_ALLOWED) {
        context_send_text(&context, sv("405 Method Not Allowed"),
                          sv("method not allowed\n"));
    } else if (!context.response_sent) {
        context_send_text(&context, sv("204 No Content"), (StringView){0});
    }
    database_close(&database);
    arena_destroy(&arena);
    close(client_fd);
}

static void ceasy_home(Context *context) {
    context_send_text(context, sv("200 OK"), sv("Ceasy\n"));
}

__attribute__((weak)) void routes(Router *router) {
    route_get(router, "/", ceasy_home);
}

void ceasy_run(int argc, char **argv) {
    HttpServer server;
    Router *router = router_default();

    (void)argc;
    (void)argv;
    router_reset(router);
    routes(router);
    if (!http_server_start(&server, 3000)) {
        fprintf(stderr, "could not start HTTP server: %s\n",
                http_server_error(&server));
        return;
    }
    signal(SIGCHLD, SIG_IGN);
    while (true) {
        int client_fd = http_server_accept(&server);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "could not accept HTTP client: %s\n",
                    http_server_error(&server));
            break;
        }
        pid_t child_pid = fork();
        if (child_pid < 0) {
            close(client_fd);
            continue;
        }
        if (child_pid == 0) {
            http_server_close(&server);
            ceasy_handle_client(client_fd, router);
            _exit(0);
        }
        close(client_fd);
    }
    http_server_close(&server);
}

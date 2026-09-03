#include "ceasy/ceasy.h"

#include <ceasy/config/config.h>
#include <ceasy/security/csrf.h>

#include <sodium.h>

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

static bool ceasy_prepare_response(Context *context) {
    if (context == NULL || !session_commit(context)) {
        return false;
    }
    if (context->arena == NULL) {
        return true;
    }
    return context_set_header(context, sv("X-Content-Type-Options"),
                              sv("nosniff")) &&
           context_set_header(context, sv("Referrer-Policy"),
                              sv("strict-origin-when-cross-origin")) &&
           context_set_header(context, sv("X-Frame-Options"),
                              sv("SAMEORIGIN")) &&
           (context->request_id.length == 0 ||
            context_set_header(context, sv("X-Request-ID"),
                               context->request_id));
}

static bool ceasy_append_response_headers(String *header, Context *context) {
    for (size_t index = 0; index < context->response_header_count; index++) {
        ResponseHeader *item = &context->response_headers[index];
        if (!string_append(header, item->name) ||
            !string_append(header, sv(": ")) ||
            !string_append(header, item->value) ||
            !string_append(header, sv("\r\n"))) {
            return false;
        }
    }
    return true;
}

bool context_send_bytes(Context *context, StringView status,
                        StringView content_type, const void *data,
                        size_t length) {
    String header;
    bool sent;

    if (context == NULL || context->response_sent || context->client_fd < 0 ||
        status.data == NULL || content_type.data == NULL ||
        (length > 0 && data == NULL)) {
        return false;
    }
    if (!ceasy_prepare_response(context)) {
        return false;
    }
    header = context->arena != NULL ? string_new_in(context->arena)
                                    : string_new_heap();
    if (!string_append(&header, sv("HTTP/1.1 ")) ||
        !string_append(&header, status) ||
        !string_append(&header, sv("\r\nContent-Type: ")) ||
        !string_append(&header, content_type) ||
        !string_append(&header, sv("\r\n")) ||
        !ceasy_append_response_headers(&header, context) ||
        !string_append_format(&header,
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n\r\n",
                              length)) {
        string_destroy(&header);
        return false;
    }
    sent = ceasy_send_all(context->client_fd, string_as_view(&header));
    if (sent && length > 0) {
        sent = ceasy_send_all(
            context->client_fd,
            (StringView){.data = (const char *)data, .length = length});
    }
    string_destroy(&header);
    if (sent) {
        context->response_sent = true;
    }
    return sent;
}

bool context_send_response(Context *context, StringView status,
                           StringView content_type, StringView body) {
    return context_send_bytes(context, status, content_type, body.data,
                              body.length);
}

bool context_send_text(Context *context, StringView status, StringView body) {
    return context_send_bytes(context, status, sv("text/plain; charset=utf-8"),
                              body.data, body.length);
}

bool context_send_html(Context *context, StringView status, StringView body) {
    return context_send_bytes(context, status, sv("text/html; charset=utf-8"),
                              body.data, body.length);
}

bool context_redirect(Context *context, StringView location) {
    String header;
    bool sent;

    if (context == NULL || context->response_sent || context->client_fd < 0 ||
        location.data == NULL) {
        return false;
    }
    if (!ceasy_prepare_response(context)) {
        return false;
    }
    header = context->arena != NULL ? string_new_in(context->arena)
                                    : string_new_heap();
    if (!string_append(&header, sv("HTTP/1.1 303 See Other\r\nLocation: ")) ||
        !string_append(&header, location) ||
        !string_append(&header, sv("\r\n")) ||
        !ceasy_append_response_headers(&header, context) ||
        !string_append(&header, sv("Content-Length: 0\r\n"
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
    String database_path;

    memset(&context, 0, sizeof(context));
    context.client_fd = client_fd;
    if (!arena_init(&arena, 64 * 1024)) {
        close(client_fd);
        return;
    }
    context.arena = &arena;
    database_path = string_from_in(&arena, ceasy_database_path());
    if (sodium_init() >= 0) {
        unsigned char request_id_bytes[8];
        char request_id[32];

        randombytes_buf(request_id_bytes, sizeof(request_id_bytes));
        snprintf(request_id, sizeof(request_id),
                 "%02x%02x%02x%02x%02x%02x%02x%02x", request_id_bytes[0],
                 request_id_bytes[1], request_id_bytes[2], request_id_bytes[3],
                 request_id_bytes[4], request_id_bytes[5], request_id_bytes[6],
                 request_id_bytes[7]);
        String request_id_copy =
            string_from_in(&arena, stringv_from_cstr(request_id));
        context.request_id = string_as_view(&request_id_copy);
    }
    if (database_path.data == NULL ||
        !database_open(&database, string_cstr(&database_path))) {
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

    if ((context.request.method == HTTP_METHOD_POST ||
         context.request.method == HTTP_METHOD_PATCH ||
         context.request.method == HTTP_METHOD_PUT ||
         context.request.method == HTTP_METHOD_DELETE) &&
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
        if (context.request.method == HTTP_METHOD_POST ||
            context.request.method == HTTP_METHOD_PATCH ||
            context.request.method == HTTP_METHOD_PUT ||
            context.request.method == HTTP_METHOD_DELETE) {
            if (!csrf_verify(&context, context_form(&context, sv("_csrf")))) {
                context_send_text(&context, sv("403 Forbidden"),
                                  sv("forbidden\n"));
                database_close(&database);
                arena_destroy(&arena);
                close(client_fd);
                return;
            }
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
        if (context.request.method == HTTP_METHOD_GET &&
            !asset_serve(&context, context.request.path)) {
            context_send_text(&context, sv("404 Not Found"), sv("not found\n"));
        }
    } else if (dispatch_result == ROUTER_METHOD_NOT_ALLOWED) {
        context_send_text(&context, sv("405 Method Not Allowed"),
                          sv("method not allowed\n"));
    } else if (!context.response_sent) {
        context_send_text(&context, sv("204 No Content"), (StringView){0});
    }
    fprintf(stderr, "[%.*s] %s %.*s\n", (int)context.request_id.length,
            context.request_id.data == NULL ? "" : context.request_id.data,
            ceasy_method_name(context.request.method),
            (int)context.request.path.length, context.request.path.data);
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
    if (!ceasy_config_init()) {
        fprintf(stderr, "invalid Ceasy configuration: %s\n",
                ceasy_config_error());
        return;
    }
    router_reset(router);
    routes(router);
    if (!http_server_start(&server, ceasy_port())) {
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

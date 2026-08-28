#include "ceasy.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static bool ceasy_read_request(int client_fd, char *buffer,
                               size_t buffer_size) {
    size_t bytes_read = 0;

    while (bytes_read + 1 < buffer_size) {
        ssize_t result = recv(client_fd, buffer + bytes_read,
                              buffer_size - bytes_read - 1, 0);

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

static bool ceasy_send_all(int client_fd, const char *data, size_t length) {
    size_t bytes_sent = 0;

    while (bytes_sent < length) {
        ssize_t result =
            send(client_fd, data + bytes_sent, length - bytes_sent, 0);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (result == 0) {
            return false;
        }

        bytes_sent += (size_t)result;
    }

    return true;
}

static const char *ceasy_row_value(const DatabaseRows *rows, int row_index,
                                   int column_index) {
    const char *value = rows->rows[row_index][column_index];

    return value != NULL ? value : "";
}

static char *ceasy_render_posts(const DatabaseRows *rows) {
    size_t body_length = 0;
    size_t body_offset = 0;
    char *body;

    for (int row_index = 0; row_index < rows->row_count; row_index++) {
        int row_length = snprintf(NULL, 0,
                                  "Post %s\nTitle: %s\nContent: %s\nCreated "
                                  "at: %s\nUpdated at: %s\n\n",
                                  ceasy_row_value(rows, row_index, 0),
                                  ceasy_row_value(rows, row_index, 1),
                                  ceasy_row_value(rows, row_index, 2),
                                  ceasy_row_value(rows, row_index, 3),
                                  ceasy_row_value(rows, row_index, 4));

        if (row_length < 0) {
            return NULL;
        }
        body_length += (size_t)row_length;
    }

    body = malloc(body_length + 1);
    if (body == NULL) {
        return NULL;
    }

    for (int row_index = 0; row_index < rows->row_count; row_index++) {
        body_offset +=
            (size_t)snprintf(body + body_offset, body_length - body_offset + 1,
                             "Post %s\nTitle: %s\nContent: %s\nCreated at: "
                             "%s\nUpdated at: %s\n\n",
                             ceasy_row_value(rows, row_index, 0),
                             ceasy_row_value(rows, row_index, 1),
                             ceasy_row_value(rows, row_index, 2),
                             ceasy_row_value(rows, row_index, 3),
                             ceasy_row_value(rows, row_index, 4));
    }

    body[body_length] = '\0';
    return body;
}

bool context_send_response(Context *context, const char *status,
                           const char *content_type, const char *body) {
    char header[256];
    int header_length;

    if (context == NULL || status == NULL || content_type == NULL ||
        body == NULL || context->client_fd < 0) {
        return false;
    }

    header_length = snprintf(header, sizeof(header),
                             "HTTP/1.1 %s\r\nContent-Type: %s\r\n"
                             "Content-Length: %zu\r\nConnection: close\r\n"
                             "\r\n",
                             status, content_type, strlen(body));

    if (header_length < 0 || (size_t)header_length >= sizeof(header)) {
        return false;
    }

    if (!ceasy_send_all(context->client_fd, header, (size_t)header_length) ||
        !ceasy_send_all(context->client_fd, body, strlen(body))) {
        return false;
    }

    context->response_sent = true;
    return true;
}

bool context_send_text(Context *context, const char *status, const char *body) {
    return context_send_response(context, status, "text/plain", body);
}

static bool ceasy_parse_request_line(const char *request, char *method,
                                     size_t method_size, char *path,
                                     size_t path_size) {
    char version[16];
    int parsed;

    parsed = sscanf(request, "%15s %4095s %15s", method, path, version);
    if (parsed != 3 || strlen(method) + 1 > method_size ||
        strlen(path) + 1 > path_size || strcmp(version, "HTTP/1.1") != 0) {
        return false;
    }

    char *query = strchr(path, '?');
    if (query != NULL) {
        *query = '\0';
    }

    return path[0] == '/';
}

static void ceasy_handle_client(int client_fd, Database *database,
                                Router *router) {
    Context context;
    char method[16];
    char path[CEASY_REQUEST_SIZE];
    RouterResult dispatch_result;

    memset(&context, 0, sizeof(context));
    context.client_fd = client_fd;
    context.database = database;

    if (!ceasy_read_request(client_fd, context.request,
                            sizeof(context.request))) {
        close(client_fd);
        return;
    }

    if (!ceasy_parse_request_line(context.request, method, sizeof(method), path,
                                  sizeof(path))) {
        context_send_text(&context, "400 Bad Request", "malformed request\n");
        close(client_fd);
        return;
    }

    dispatch_result = router_dispatch(router, method, path, &context);
    if (dispatch_result == ROUTER_NOT_FOUND) {
        context_send_text(&context, "404 Not Found", "not found\n");
    } else if (dispatch_result == ROUTER_METHOD_NOT_ALLOWED) {
        context_send_text(&context, "405 Method Not Allowed",
                          "method not allowed\n");
    } else if (!context.response_sent) {
        context_send_text(&context, "204 No Content", "");
    }

    close(client_fd);
}

static void ceasy_home(Context *context) {
    context_send_text(context, "200 OK", "Ceasy\n");
}

static void ceasy_posts_index(Context *context) {
    DatabaseRows posts;
    char *body;

    if (!database_read(context->database,
                       "SELECT id, title, content, created_at, updated_at "
                       "FROM posts ORDER BY id",
                       &posts)) {
        context_send_text(context, "500 Internal Server Error",
                          "could not read posts\n");
        return;
    }

    body = ceasy_render_posts(&posts);
    database_rows_free(&posts);
    if (body == NULL) {
        context_send_text(context, "500 Internal Server Error",
                          "could not render posts\n");
        return;
    }

    context_send_text(context, "200 OK", body);
    free(body);
}

__attribute__((weak)) void routes(Router *router) {
    route_get(router, "/", ceasy_home);
    route_get(router, "/posts", ceasy_posts_index);
}

void ceasy_run(int argc, char **argv) {
    Database database;
    HttpServer server;
    Router *router = router_default();

    (void)argc;
    (void)argv;

    router_reset(router);
    routes(router);

    if (!database_open(&database, "db/development.sqlite3")) {
        fprintf(stderr, "could not open database: %s\n",
                database_error(&database));
        return;
    }

    if (!http_server_start(&server, 3000)) {
        fprintf(stderr, "could not start HTTP server: %s\n",
                http_server_error(&server));
        database_close(&database);
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
            ceasy_handle_client(client_fd, &database, router);
            _exit(0);
        }

        close(client_fd);
    }

    http_server_close(&server);
    database_close(&database);
}

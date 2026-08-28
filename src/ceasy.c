#include "ceasy.h"

#include <errno.h>
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

static bool ceasy_send_response(int client_fd, const char *status,
                                const char *body) {
    char header[256];
    int header_length = snprintf(
        header, sizeof(header),
        "HTTP/1.1 %s\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        status, strlen(body));

    if (header_length < 0 || (size_t)header_length >= sizeof(header)) {
        return false;
    }

    return ceasy_send_all(client_fd, header, (size_t)header_length) &&
           ceasy_send_all(client_fd, body, strlen(body));
}

static void ceasy_handle_client(int client_fd, Database *database) {
    char request[4096];
    DatabaseRows posts;
    char *body;

    if (!ceasy_read_request(client_fd, request, sizeof(request))) {
        close(client_fd);
        return;
    }

    if (strncmp(request, "GET ", 4) != 0) {
        ceasy_send_response(client_fd, "405 Method Not Allowed", "");
        close(client_fd);
        return;
    }

    if (!database_read(database,
                       "SELECT id, title, content, created_at, updated_at "
                       "FROM posts ORDER BY id",
                       &posts)) {
        ceasy_send_response(client_fd, "500 Internal Server Error",
                            "could not read posts\n");
        close(client_fd);
        return;
    }

    body = ceasy_render_posts(&posts);
    database_rows_free(&posts);
    if (body == NULL) {
        ceasy_send_response(client_fd, "500 Internal Server Error",
                            "could not render posts\n");
        close(client_fd);
        return;
    }

    ceasy_send_response(client_fd, "200 OK", body);
    free(body);
    close(client_fd);
}

void ceasy_run(int argc, char **argv) {
    Database database;
    HttpServer server;

    (void)argc;
    (void)argv;

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

        ceasy_handle_client(client_fd, &database);
    }

    http_server_close(&server);
    database_close(&database);
}

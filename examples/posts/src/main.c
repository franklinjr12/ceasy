#include <ceasy/ceasy.h>

#include <stdio.h>
#include <stdlib.h>

static const char *post_value(const DatabaseRows *posts, int row_index,
                              int column_index) {
    const char *value = posts->rows[row_index][column_index];

    return value != NULL ? value : "";
}

static char *render_posts(const DatabaseRows *posts) {
    size_t body_length = 0;
    size_t body_offset = 0;
    char *body;

    for (int row_index = 0; row_index < posts->row_count; row_index++) {
        int row_length = snprintf(
            NULL, 0, "Post %s\nTitle: %s\nContent: %s\n\n",
            post_value(posts, row_index, 0), post_value(posts, row_index, 1),
            post_value(posts, row_index, 2));
        if (row_length < 0) {
            return NULL;
        }
        body_length += (size_t)row_length;
    }

    body = malloc(body_length + 1);
    if (body == NULL) {
        return NULL;
    }

    for (int row_index = 0; row_index < posts->row_count; row_index++) {
        body_offset += (size_t)snprintf(
            body + body_offset, body_length - body_offset + 1,
            "Post %s\nTitle: %s\nContent: %s\n\n",
            post_value(posts, row_index, 0), post_value(posts, row_index, 1),
            post_value(posts, row_index, 2));
    }

    body[body_length] = '\0';
    return body;
}

static void get_posts(Context *context) {
    DatabaseRows posts;
    char *body;

    if (!database_read(context->database,
                       "SELECT id, title, content FROM posts ORDER BY id",
                       &posts)) {
        context_send_text(context, "500 Internal Server Error",
                          "could not read posts\n");
        return;
    }

    body = render_posts(&posts);
    database_rows_free(&posts);
    if (body == NULL) {
        context_send_text(context, "500 Internal Server Error",
                          "could not render posts\n");
        return;
    }

    context_send_text(context, "200 OK", body);
    free(body);
}

void routes(Router *router) { route_get(router, "/posts", get_posts); }

int main(int argc, char **argv) {
    ceasy_run(argc, argv);
    return 0;
}

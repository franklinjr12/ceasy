#include "posts_controller.h"

#include <ceasy/rendering/html.h>

#include <stdint.h>
#include <string.h>

typedef struct {
    int64_t id;
    String title;
    String content;
    String created_at;
    String updated_at;
} Post;

typedef enum {
    POST_FIND_ERROR = 0,
    POST_FIND_MISSING,
    POST_FIND_FOUND
} PostFindResult;

static void post_init(Post *post) { memset(post, 0, sizeof(*post)); }

static PostFindResult post_find(Context *context, int64_t id, Post *post) {
    DatabaseStatement statement = {0};
    DatabaseStepResult step;

    post_init(post);
    if (!database_prepare(
            context->database, &statement,
            sv("SELECT id, title, content, created_at, updated_at "
               "FROM posts WHERE id = ?"))) {
        return POST_FIND_ERROR;
    }
    if (!database_bind_int64(&statement, 1, id)) {
        database_statement_destroy(&statement);
        return POST_FIND_ERROR;
    }
    step = database_step(&statement);
    if (step == DATABASE_STEP_DONE) {
        database_statement_destroy(&statement);
        return POST_FIND_MISSING;
    }
    if (step != DATABASE_STEP_ROW || context->arena == NULL) {
        database_statement_destroy(&statement);
        return POST_FIND_ERROR;
    }
    post->id = database_column_int64(&statement, 0);
    post->title =
        string_from_in(context->arena, database_column_text(&statement, 1));
    post->content =
        string_from_in(context->arena, database_column_text(&statement, 2));
    post->created_at =
        string_from_in(context->arena, database_column_text(&statement, 3));
    post->updated_at =
        string_from_in(context->arena, database_column_text(&statement, 4));
    database_statement_destroy(&statement);
    if ((post->title.length > 0 && post->title.data == NULL) ||
        (post->content.length > 0 && post->content.data == NULL) ||
        (post->created_at.length > 0 && post->created_at.data == NULL) ||
        (post->updated_at.length > 0 && post->updated_at.data == NULL)) {
        return POST_FIND_ERROR;
    }
    return POST_FIND_FOUND;
}

static bool post_id(Context *context, int64_t *id) {
    StringView value = context_param(context, sv("id"));

    return stringv_parse_int64(value, id) && *id > 0;
}

static bool append_layout_start(String *html, StringView title) {
    return string_append(html, sv("<!doctype html><html><head><meta "
                                  "charset=\"utf-8\"><title>")) &&
           html_escape_append(html, title) &&
           string_append(html, sv("</title></head><body>"));
}

static bool append_layout_end(String *html) {
    return string_append(html, sv("</body></html>"));
}

static bool send_rendered(Context *context, String *html) {
    return context_send_html(context, sv("200 OK"), string_as_view(html));
}

static void post_error(Context *context, StringView status, StringView body) {
    context_send_text(context, status, body);
}

void posts_index(Context *context) {
    DatabaseStatement statement = {0};
    String html = string_new_in(context->arena);
    DatabaseStepResult step;
    bool success = append_layout_start(&html, sv("Posts")) &&
                   string_append(&html, sv("<h1>Posts</h1><a href=\"/posts/"
                                           "new\">New post</a><ul>"));

    if (!success ||
        !database_prepare(
            context->database, &statement,
            sv("SELECT id, title, content FROM posts ORDER BY id"))) {
        string_destroy(&html);
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    while ((step = database_step(&statement)) == DATABASE_STEP_ROW) {
        StringView title = database_column_text(&statement, 1);
        StringView content = database_column_text(&statement, 2);
        long long id = (long long)database_column_int64(&statement, 0);

        success =
            string_append_format(
                &html, "<li><article><h2><a href=\"/posts/%lld\">", id) &&
            html_escape_append(&html, title) &&
            string_append(&html, sv("</a></h2><p>")) &&
            html_escape_append(&html, content) &&
            string_append(&html, sv("</p><a href=\"/posts/")) &&
            string_append_format(&html, "%lld/edit\">Edit</a> ", id) &&
            string_append_format(&html,
                                 "<form method=\"POST\" action=\"/posts/%lld\" "
                                 "style=\"display:inline\"><input "
                                 "type=\"hidden\" name=\"_method\" "
                                 "value=\"DELETE\"><button>Delete</button></"
                                 "form></article></li>",
                                 id);
        if (!success) {
            break;
        }
    }
    if (step == DATABASE_STEP_ERROR) {
        success = false;
    }
    database_statement_destroy(&statement);
    success = success && string_append(&html, sv("</ul>")) &&
              append_layout_end(&html);
    if (!success) {
        string_destroy(&html);
        post_error(context, sv("500 Internal Server Error"),
                   sv("could not render posts\n"));
        return;
    }
    send_rendered(context, &html);
    string_destroy(&html);
}

void posts_show(Context *context) {
    int64_t id;
    Post post;
    PostFindResult result;
    String html;

    if (!post_id(context, &id)) {
        post_error(context, sv("400 Bad Request"), sv("invalid post id\n"));
        return;
    }
    result = post_find(context, id, &post);
    if (result == POST_FIND_MISSING) {
        post_error(context, sv("404 Not Found"), sv("post not found\n"));
        return;
    }
    if (result != POST_FIND_FOUND) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    html = string_new_in(context->arena);
    if (!append_layout_start(&html, string_as_view(&post.title)) ||
        !string_append(&html, sv("<h1>")) ||
        !html_escape_append(&html, string_as_view(&post.title)) ||
        !string_append(&html, sv("</h1><p>")) ||
        !html_escape_append(&html, string_as_view(&post.content)) ||
        !string_append_format(
            &html,
            "</p><a href=\"/posts/%lld/edit\">Edit</a> <form method=\"POST\" "
            "action=\"/posts/%lld\"><input type=\"hidden\" name=\"_method\" "
            "value=\"DELETE\"><button>Delete</button></form><p><a "
            "href=\"/posts\">Back</a></p>",
            (long long)id, (long long)id) ||
        !append_layout_end(&html)) {
        string_destroy(&html);
        post_error(context, sv("500 Internal Server Error"),
                   sv("could not render post\n"));
        return;
    }
    send_rendered(context, &html);
    string_destroy(&html);
}

static bool append_post_form(String *html, StringView heading,
                             StringView action, StringView title,
                             StringView content, StringView method) {
    return append_layout_start(html, heading) &&
           string_append(html, sv("<h1>")) &&
           html_escape_append(html, heading) &&
           string_append(html, sv("</h1><form method=\"POST\" action=\"")) &&
           html_escape_append(html, action) &&
           string_append(
               html,
               sv("\"><input type=\"hidden\" name=\"_method\" value=\"")) &&
           html_escape_append(html, method) &&
           string_append(
               html,
               sv("\"><label>Title</label><input name=\"title\" value=\"")) &&
           html_escape_append(html, title) &&
           string_append(
               html,
               sv("\"><label>Content</label><textarea name=\"content\">")) &&
           html_escape_append(html, content) &&
           string_append(html, sv("</textarea><button>Save</button></form>")) &&
           string_append(html, sv("<p><a href=\"/posts\">Back</a></p>")) &&
           append_layout_end(html);
}

void posts_new(Context *context) {
    String html = string_new_in(context->arena);

    if (!append_post_form(&html, sv("New post"), sv("/posts"), (StringView){0},
                          (StringView){0}, sv(""))) {
        string_destroy(&html);
        post_error(context, sv("500 Internal Server Error"),
                   sv("could not render form\n"));
        return;
    }
    send_rendered(context, &html);
    string_destroy(&html);
}

void posts_create(Context *context) {
    StringView title;
    StringView content;
    DatabaseStatement statement = {0};

    if (!context_parse_form(context)) {
        post_error(context, sv("400 Bad Request"), sv("malformed form\n"));
        return;
    }
    title = context_form(context, sv("title"));
    content = context_form(context, sv("content"));
    if (stringv_empty(title)) {
        post_error(context, sv("400 Bad Request"), sv("title is required\n"));
        return;
    }
    if (!database_prepare(
            context->database, &statement,
            sv("INSERT INTO posts (title, content, created_at, updated_at) "
               "VALUES (?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)")) ||
        !database_bind_text(&statement, 1, title) ||
        !database_bind_text(&statement, 2, content) ||
        !database_execute(&statement)) {
        database_statement_destroy(&statement);
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    database_statement_destroy(&statement);
    context_redirect(context, sv("/posts"));
}

void posts_edit(Context *context) {
    int64_t id;
    Post post;
    PostFindResult result;
    String action;
    String html;

    if (!post_id(context, &id)) {
        post_error(context, sv("400 Bad Request"), sv("invalid post id\n"));
        return;
    }
    result = post_find(context, id, &post);
    if (result == POST_FIND_MISSING) {
        post_error(context, sv("404 Not Found"), sv("post not found\n"));
        return;
    }
    if (result != POST_FIND_FOUND) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    action = string_format_in(context->arena, "/posts/%lld", (long long)id);
    html = string_new_in(context->arena);
    if (action.data == NULL ||
        !append_post_form(&html, sv("Edit post"), string_as_view(&action),
                          string_as_view(&post.title),
                          string_as_view(&post.content), sv("PATCH"))) {
        string_destroy(&html);
        post_error(context, sv("500 Internal Server Error"),
                   sv("could not render form\n"));
        return;
    }
    send_rendered(context, &html);
    string_destroy(&html);
}

void posts_update(Context *context) {
    int64_t id;
    Post post;
    DatabaseStatement statement = {0};
    StringView title;
    StringView content;
    String location;
    PostFindResult result;

    if (!post_id(context, &id)) {
        post_error(context, sv("400 Bad Request"), sv("invalid post id\n"));
        return;
    }
    result = post_find(context, id, &post);
    if (result == POST_FIND_MISSING) {
        post_error(context, sv("404 Not Found"), sv("post not found\n"));
        return;
    }
    if (result != POST_FIND_FOUND || !context_parse_form(context)) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    title = context_form(context, sv("title"));
    content = context_form(context, sv("content"));
    if (stringv_empty(title)) {
        post_error(context, sv("400 Bad Request"), sv("title is required\n"));
        return;
    }
    if (!database_prepare(context->database, &statement,
                          sv("UPDATE posts SET title = ?, content = ?, "
                             "updated_at = CURRENT_TIMESTAMP WHERE id = ?")) ||
        !database_bind_text(&statement, 1, title) ||
        !database_bind_text(&statement, 2, content) ||
        !database_bind_int64(&statement, 3, id) ||
        !database_execute(&statement)) {
        database_statement_destroy(&statement);
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    database_statement_destroy(&statement);
    location = string_format_in(context->arena, "/posts/%lld", (long long)id);
    if (location.data == NULL) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("could not redirect\n"));
        return;
    }
    context_redirect(context, string_as_view(&location));
}

void posts_destroy(Context *context) {
    int64_t id;
    Post post;
    DatabaseStatement statement = {0};
    PostFindResult result;

    if (!post_id(context, &id)) {
        post_error(context, sv("400 Bad Request"), sv("invalid post id\n"));
        return;
    }
    result = post_find(context, id, &post);
    if (result == POST_FIND_MISSING) {
        post_error(context, sv("404 Not Found"), sv("post not found\n"));
        return;
    }
    if (result != POST_FIND_FOUND ||
        !database_prepare(context->database, &statement,
                          sv("DELETE FROM posts WHERE id = ?")) ||
        !database_bind_int64(&statement, 1, id) ||
        !database_execute(&statement)) {
        database_statement_destroy(&statement);
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    database_statement_destroy(&statement);
    context_redirect(context, sv("/posts"));
}

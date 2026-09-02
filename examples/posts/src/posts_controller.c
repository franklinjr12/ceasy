#include "posts_controller.h"

#include <stdint.h>

static bool post_id(Context *context, int64_t *id) {
    StringView value = context_param(context, sv("id"));

    return stringv_parse_int64(value, id) && *id > 0;
}

static void post_error(Context *context, StringView status, StringView body) {
    context_send_text(context, status, body);
}

void posts_index(Context *context) {
    PostArray posts = {0};
    if (!post_all(context, &posts) ||
        !view_set(context, sv("page_title"), view_string(sv("Posts"))) ||
        !view_set(context, sv("posts"), post_array_view(posts))) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    render(context, sv("posts/index"));
}

void posts_show(Context *context) {
    int64_t id;
    Post *post = NULL;
    ModelResult result;
    if (!post_id(context, &id)) {
        post_error(context, sv("400 Bad Request"), sv("invalid post id\n"));
        return;
    }
    result = post_find(context, id, &post);
    if (result == MODEL_RESULT_NOT_FOUND) {
        post_error(context, sv("404 Not Found"), sv("post not found\n"));
        return;
    }
    if (result != MODEL_RESULT_OK) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    if (!view_set(context, sv("page_title"),
                  view_string(string_as_view(&post->title))) ||
        !view_set(context, sv("post"), post_view(post))) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("could not prepare post\n"));
        return;
    }
    render(context, sv("posts/show"));
}

void posts_new(Context *context) {
    if (!view_set(context, sv("page_title"), view_string(sv("New post"))) ||
        !view_set(context, sv("form_action"), view_string(sv("/posts"))) ||
        !view_set(context, sv("editing"), view_bool(false)) ||
        !view_set(context, sv("title"), view_string((StringView){0})) ||
        !view_set(context, sv("content"), view_string((StringView){0}))) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("could not render form\n"));
        return;
    }
    render(context, sv("posts/new"));
}

void posts_create(Context *context) {
    StringView title;
    StringView content;
    Post post = {0};

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
    post.title = string_from_in(context->arena, title);
    post.content = string_from_in(context->arena, content);
    if (!post_insert(context, &post)) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    context_redirect(context, sv("/posts"));
}

void posts_edit(Context *context) {
    int64_t id;
    Post *post = NULL;
    ModelResult result;
    String action;

    if (!post_id(context, &id)) {
        post_error(context, sv("400 Bad Request"), sv("invalid post id\n"));
        return;
    }
    result = post_find(context, id, &post);
    if (result == MODEL_RESULT_NOT_FOUND) {
        post_error(context, sv("404 Not Found"), sv("post not found\n"));
        return;
    }
    if (result != MODEL_RESULT_OK) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    action = string_format_in(context->arena, "/posts/%lld", (long long)id);
    if (action.data == NULL ||
        !view_set(context, sv("page_title"), view_string(sv("Edit post"))) ||
        !view_set(context, sv("post"), post_view(post)) ||
        !view_set(context, sv("form_action"),
                  view_string(string_as_view(&action))) ||
        !view_set(context, sv("editing"), view_bool(true)) ||
        !view_set(context, sv("title"),
                  view_string(string_as_view(&post->title))) ||
        !view_set(context, sv("content"),
                  view_string(string_as_view(&post->content)))) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("could not render form\n"));
        return;
    }
    render(context, sv("posts/edit"));
}

void posts_update(Context *context) {
    int64_t id;
    Post *post = NULL;
    StringView title;
    StringView content;
    String location;
    ModelResult result;

    if (!post_id(context, &id)) {
        post_error(context, sv("400 Bad Request"), sv("invalid post id\n"));
        return;
    }
    result = post_find(context, id, &post);
    if (result == MODEL_RESULT_NOT_FOUND) {
        post_error(context, sv("404 Not Found"), sv("post not found\n"));
        return;
    }
    if (result != MODEL_RESULT_OK || !context_parse_form(context)) {
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
    post->title = string_from_in(context->arena, title);
    post->content = string_from_in(context->arena, content);
    if (post_update(context, post) != MODEL_RESULT_OK) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
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
    Post *post = NULL;
    ModelResult result;

    if (!post_id(context, &id)) {
        post_error(context, sv("400 Bad Request"), sv("invalid post id\n"));
        return;
    }
    result = post_find(context, id, &post);
    if (result == MODEL_RESULT_NOT_FOUND) {
        post_error(context, sv("404 Not Found"), sv("post not found\n"));
        return;
    }
    if (result != MODEL_RESULT_OK ||
        post_destroy(context, post) != MODEL_RESULT_OK) {
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    context_redirect(context, sv("/posts"));
}

#include "posts_controller.h"

#include <ceasy/rendering/html.h>

#include <stdint.h>

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
    String html = string_new_in(context->arena);
    PostArray posts = {0};
    bool success = append_layout_start(&html, sv("Posts")) &&
                   string_append(&html, sv("<h1>Posts</h1><a href=\"/posts/"
                                           "new\">New post</a><ul>"));

    if (!success || !post_all(context, &posts)) {
        string_destroy(&html);
        post_error(context, sv("500 Internal Server Error"),
                   sv("database error\n"));
        return;
    }
    for (size_t index = 0; index < posts.length; index++) {
        Post *post = &posts.items[index];
        long long id = (long long)post->id;

        success =
            string_append_format(
                &html, "<li><article><h2><a href=\"/posts/%lld\">", id) &&
            html_escape_append(&html, string_as_view(&post->title)) &&
            string_append(&html, sv("</a></h2><p>")) &&
            html_escape_append(&html, string_as_view(&post->content)) &&
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
    Post *post = NULL;
    ModelResult result;
    String html;

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
    html = string_new_in(context->arena);
    if (!append_layout_start(&html, string_as_view(&post->title)) ||
        !string_append(&html, sv("<h1>")) ||
        !html_escape_append(&html, string_as_view(&post->title)) ||
        !string_append(&html, sv("</h1><p>")) ||
        !html_escape_append(&html, string_as_view(&post->content)) ||
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
    String html;

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
    html = string_new_in(context->arena);
    if (action.data == NULL ||
        !append_post_form(&html, sv("Edit post"), string_as_view(&action),
                          string_as_view(&post->title),
                          string_as_view(&post->content), sv("PATCH"))) {
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

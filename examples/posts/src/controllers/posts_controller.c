#include "posts_controller.h"

#include "../models/post.h"
#include "../models/user.h"
#include "../queries/blog_queries.h"
#include "concerns/blog_controller.h"

#include <ceasy/validation/validation.h>

#include <time.h>

static String blog_timestamp(Context *context) {
    time_t now = time(NULL);
    struct tm utc;
    char value[32];

    if (now == (time_t)-1 || gmtime_r(&now, &utc) == NULL ||
        strftime(value, sizeof(value), "%Y-%m-%d %H:%M:%S", &utc) == 0)
        return (String){0};
    return string_from_in(context->arena, stringv_from_cstr(value));
}

static bool validate_post(StringView title, StringView summary,
                          StringView content, ValidationErrors *errors) {
    if (!validation_present(title) ||
        !validation_length_between(stringv_trim(title), 3, 200))
        validation_errors_add(
            errors, sv("title"),
            sv("Title must be between 3 and 200 characters."));
    if (!validation_present(summary) ||
        !validation_length_between(stringv_trim(summary), 10, 500))
        validation_errors_add(
            errors, sv("summary"),
            sv("Summary must be between 10 and 500 characters."));
    if (!validation_present(content) ||
        !validation_length_at_most(content, 50000))
        validation_errors_add(
            errors, sv("content"),
            sv("Content is required and must be at most 50,000 characters."));
    return !validation_errors_any(errors);
}

static int64_t page_number(Context *context) {
    int64_t page;
    return stringv_parse_int64(context_query(context, sv("page")), &page) &&
                   page > 0 && page < 1000000
               ? page
               : 1;
}

void posts_index(Context *context) {
    PostCardArray cards = {0};
    bool has_next;
    StringView search = context_query(context, sv("q"));
    int64_t page = page_number(context);
    if (!post_cards_query(context, search, page, &cards, &has_next) ||
        !view_set(context, sv("page_title"),
                  view_string(sv("Browse articles"))) ||
        !view_set(context, sv("posts"), post_card_array_view(cards)) ||
        !view_set(context, sv("current_page"), view_int64(page)) ||
        !view_set(context, sv("has_previous"), view_bool(page > 1)) ||
        !view_set(context, sv("has_next"), view_bool(has_next)) ||
        !view_set(context, sv("previous_page"), view_int64(page - 1)) ||
        !view_set(context, sv("next_page"), view_int64(page + 1)) ||
        !view_set(context, sv("query"), view_string(search))) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    blog_render(context, sv("posts/index"), sv("200 OK"));
}

void posts_show(Context *context) {
    int64_t id;
    Post *post = NULL;
    User *viewer = blog_current_user(context);
    if (!blog_id(context, &id) ||
        post_find(context, id, &post) != MODEL_RESULT_OK || post == NULL ||
        (!post->published && (viewer == NULL || (viewer->id != post->user_id &&
                                                 !viewer->is_admin)))) {
        blog_error(context, sv("404 Not Found"),
                   sv("We couldn't find that page.\n"));
        return;
    }
    if (!view_set(context, sv("comment"), view_string((StringView){0})) ||
        !blog_render_post_show(context, post, viewer, sv("200 OK"))) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
}

void posts_new(Context *context) {
    if (!require_authenticated_user(context))
        return;
    view_set(context, sv("page_title"), view_string(sv("Write an article")));
    view_set(context, sv("form_action"), view_string(sv("/posts")));
    view_set(context, sv("editing"), view_bool(false));
    view_set(context, sv("title"), view_string((StringView){0}));
    view_set(context, sv("summary"), view_string((StringView){0}));
    view_set(context, sv("content"), view_string((StringView){0}));
    blog_render(context, sv("posts/new"), sv("200 OK"));
}

void posts_create(Context *context) {
    User *user;
    Post post = {0};
    ValidationErrors errors;
    String title, summary;
    StringView content;
    bool publish;
    if (!require_authenticated_user(context) || !context_parse_form(context))
        return;
    user = blog_current_user(context);
    title = normalized(context, context_form(context, sv("title")), false);
    summary = normalized(context, context_form(context, sv("summary")), false);
    content = context_form(context, sv("content"));
    validation_errors_init(&errors, context->arena);
    if (!validate_post(string_as_view(&title), string_as_view(&summary),
                       content, &errors)) {
        view_set(context, sv("page_title"),
                 view_string(sv("Write an article")));
        view_set(context, sv("form_action"), view_string(sv("/posts")));
        view_set(context, sv("editing"), view_bool(false));
        view_set(context, sv("title"), view_string(string_as_view(&title)));
        view_set(context, sv("summary"), view_string(string_as_view(&summary)));
        view_set(context, sv("content"), view_string(content));
        form_errors(context, &errors);
        blog_render(context, sv("posts/new"), sv("422 Unprocessable Content"));
        return;
    }
    publish = stringv_equal(context_form(context, sv("intent")), sv("publish"));
    post.user_id = user->id;
    post.title = title;
    post.summary = summary;
    post.content = string_from_in(context->arena, content);
    post.published = publish;
    post.published_at = publish ? blog_timestamp(context) : (String){0};
    if (!post_insert(context, &post)) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    flash_set(context, sv("success"),
              publish ? sv("Post published.") : sv("Draft saved."));
    {
        String location =
            string_format_in(context->arena, "/posts/%lld", (long long)post.id);
        context_redirect(context, string_as_view(&location));
    }
}

static bool load_owned_post(Context *context, Post **post, User **viewer) {
    int64_t id;
    if (!blog_id(context, &id) ||
        post_find(context, id, post) != MODEL_RESULT_OK || *post == NULL)
        return false;
    *viewer = blog_current_user(context);
    return *viewer != NULL &&
           ((*viewer)->is_admin || (*viewer)->id == (*post)->user_id);
}

void posts_edit(Context *context) {
    Post *post = NULL;
    User *viewer = NULL;
    if (!require_authenticated_user(context))
        return;
    if (!load_owned_post(context, &post, &viewer)) {
        blog_error(context, sv("404 Not Found"),
                   sv("We couldn't find that page.\n"));
        return;
    }
    {
        String action = string_format_in(context->arena, "/posts/%lld",
                                         (long long)post->id);
        view_set(context, sv("page_title"), view_string(sv("Edit article")));
        view_set(context, sv("post"), post_view(post));
        view_set(context, sv("form_action"),
                 view_string(string_as_view(&action)));
        view_set(context, sv("editing"), view_bool(true));
        view_set(context, sv("title"),
                 view_string(string_as_view(&post->title)));
        view_set(context, sv("summary"),
                 view_string(string_as_view(&post->summary)));
        view_set(context, sv("content"),
                 view_string(string_as_view(&post->content)));
    }
    blog_render(context, sv("posts/edit"), sv("200 OK"));
}

void posts_update(Context *context) {
    Post *post = NULL;
    User *viewer = NULL;
    ValidationErrors errors;
    String title, summary;
    StringView content;
    bool publish;
    if (!require_authenticated_user(context) || !context_parse_form(context))
        return;
    if (!load_owned_post(context, &post, &viewer)) {
        blog_error(context, sv("403 Forbidden"),
                   sv("You don't have permission to do that.\n"));
        return;
    }
    title = normalized(context, context_form(context, sv("title")), false);
    summary = normalized(context, context_form(context, sv("summary")), false);
    content = context_form(context, sv("content"));
    validation_errors_init(&errors, context->arena);
    if (!validate_post(string_as_view(&title), string_as_view(&summary),
                       content, &errors)) {
        view_set(context, sv("page_title"), view_string(sv("Edit article")));
        view_set(context, sv("post"), post_view(post));
        view_set(context, sv("form_action"),
                 view_string(context->request.path));
        view_set(context, sv("editing"), view_bool(true));
        view_set(context, sv("title"), view_string(string_as_view(&title)));
        view_set(context, sv("summary"), view_string(string_as_view(&summary)));
        view_set(context, sv("content"), view_string(content));
        form_errors(context, &errors);
        blog_render(context, sv("posts/edit"), sv("422 Unprocessable Content"));
        return;
    }
    publish = stringv_equal(context_form(context, sv("intent")), sv("publish"));
    post->title = title;
    post->summary = summary;
    post->content = string_from_in(context->arena, content);
    if (publish && !post->published)
        post->published_at = blog_timestamp(context);
    post->published = publish;
    if (!post_update(context, post)) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    flash_set(context, sv("success"),
              publish ? sv("Post published.") : sv("Draft saved."));
    {
        String location = string_format_in(context->arena, "/posts/%lld",
                                           (long long)post->id);
        context_redirect(context, string_as_view(&location));
    }
}

void posts_destroy(Context *context) {
    Post *post = NULL;
    User *viewer = NULL;
    if (!require_authenticated_user(context))
        return;
    if (!load_owned_post(context, &post, &viewer)) {
        blog_error(context, sv("403 Forbidden"),
                   sv("You don't have permission to do that.\n"));
        return;
    }
    if (post_destroy(context, post) != MODEL_RESULT_OK) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    flash_set(context, sv("success"), sv("Post deleted."));
    context_redirect(context, sv("/dashboard"));
}

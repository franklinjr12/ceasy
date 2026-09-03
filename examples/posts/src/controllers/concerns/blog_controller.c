#include "blog_controller.h"

#include "../../models/comment.h"
#include "../../queries/blog_queries.h"

#include <ceasy/validation/validation.h>

static bool blog_prepare_view(Context *context);

void blog_error(Context *context, StringView status, StringView message) {
    StringView path =
        stringv_starts_with(status, sv("403"))   ? sv("errors/403")
        : stringv_starts_with(status, sv("404")) ? sv("errors/404")
        : stringv_starts_with(status, sv("500")) ? sv("errors/500")
                                                 : (StringView){0};

    if (path.length > 0 &&
        view_set(context, sv("page_title"), view_string(sv("Error"))) &&
        blog_prepare_view(context) && render_status(context, path, status)) {
        return;
    }
    context_send_text(context, status, message);
}

bool blog_id(Context *context, int64_t *id) {
    return stringv_parse_int64(context_param(context, sv("id")), id) && *id > 0;
}

User *blog_current_user(Context *context) {
    int64_t id;
    User *user = NULL;
    if (!auth_user_id(context, &id) ||
        user_find(context, id, &user) != MODEL_RESULT_OK) {
        if (auth_signed_in(context))
            auth_logout(context);
        return NULL;
    }
    return user;
}

bool require_authenticated_user(Context *context) {
    if (blog_current_user(context) != NULL)
        return true;
    flash_set(context, sv("notice"), sv("Please sign in to continue."));
    context_redirect(context, sv("/login"));
    return false;
}

static bool blog_prepare_view(Context *context) {
    User *user = blog_current_user(context);
    StringView message;
    ValidationErrors empty_errors = {0};
    if (!view_set(context, sv("signed_in"), view_bool(user != NULL)) ||
        !view_set(context, sv("current_user_id"),
                  view_int64(user == NULL ? 0 : user->id)) ||
        !view_set(context, sv("current_user_name"),
                  view_string(user == NULL ? (StringView){0}
                                           : string_as_view(&user->name))))
        return false;
    {
        ViewValue existing;
#define BLOG_DEFAULT(name, value)                                              \
    (view_get(&context->view_data, (name), &existing) ||                       \
     view_set(context, (name), (value)))
        if (!BLOG_DEFAULT(sv("errors"),
                          validation_errors_view(&empty_errors)) ||
            !BLOG_DEFAULT(sv("title_error"), view_string((StringView){0})) ||
            !BLOG_DEFAULT(sv("summary_error"), view_string((StringView){0})) ||
            !BLOG_DEFAULT(sv("content_error"), view_string((StringView){0})) ||
            !BLOG_DEFAULT(sv("name_error"), view_string((StringView){0})) ||
            !BLOG_DEFAULT(sv("email_error"), view_string((StringView){0})) ||
            !BLOG_DEFAULT(sv("bio_error"), view_string((StringView){0})) ||
            !BLOG_DEFAULT(sv("password_error"), view_string((StringView){0})) ||
            !BLOG_DEFAULT(sv("confirmation_error"),
                          view_string((StringView){0})) ||
            !BLOG_DEFAULT(sv("comment_error"), view_string((StringView){0}))) {
            return false;
        }
#undef BLOG_DEFAULT
    }
    message = flash_get(context, sv("success"));
    if (!view_set(context, sv("flash_success"), view_string(message)))
        return false;
    message = flash_get(context, sv("notice"));
    if (!view_set(context, sv("flash_notice"), view_string(message)))
        return false;
    message = flash_get(context, sv("error"));
    if (!view_set(context, sv("flash_error"), view_string(message)))
        return false;
    if (user != NULL &&
        !view_set(context, sv("csrf_token"), view_string(csrf_token(context))))
        return false;
    return true;
}

bool blog_render(Context *context, StringView path, StringView status) {
    if (!blog_prepare_view(context)) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return false;
    }
    return render_status(context, path, status);
}

String normalized(Context *context, StringView value, bool lower) {
    String result = string_from_in(context->arena, stringv_trim(value));
    if (lower)
        string_lower(&result);
    return result;
}

void form_errors(Context *context, ValidationErrors *errors) {
    view_set(context, sv("errors"), validation_errors_view(errors));
    view_set(context, sv("title_error"),
             view_string(validation_error_for(errors, sv("title"))));
    view_set(context, sv("summary_error"),
             view_string(validation_error_for(errors, sv("summary"))));
    view_set(context, sv("content_error"),
             view_string(validation_error_for(errors, sv("content"))));
    view_set(context, sv("name_error"),
             view_string(validation_error_for(errors, sv("name"))));
    view_set(context, sv("email_error"),
             view_string(validation_error_for(errors, sv("email"))));
    view_set(context, sv("bio_error"),
             view_string(validation_error_for(errors, sv("bio"))));
    view_set(context, sv("password_error"),
             view_string(validation_error_for(errors, sv("password"))));
    view_set(
        context, sv("confirmation_error"),
        view_string(validation_error_for(errors, sv("password_confirmation"))));
    view_set(context, sv("comment_error"),
             view_string(validation_error_for(errors, sv("comment"))));
}

bool blog_render_post_show(Context *context, Post *post, User *viewer,
                           StringView status) {
    User *author = NULL;
    CommentViewArray comments = {0};

    if (user_find(context, post->user_id, &author) != MODEL_RESULT_OK ||
        !comments_query(context, post->id, viewer == NULL ? 0 : viewer->id,
                        viewer != NULL && viewer->is_admin, &comments) ||
        !view_set(context, sv("page_title"),
                  view_string(string_as_view(&post->title))) ||
        !view_set(context, sv("post"), post_view(post)) ||
        !view_set(context, sv("author"), user_view(author)) ||
        !view_set(context, sv("comments"), comment_array_view(comments)) ||
        !view_set(context, sv("can_manage"),
                  view_bool(viewer != NULL && (viewer->is_admin ||
                                               viewer->id == post->user_id))) ||
        !view_set(context, sv("published_label"),
                  view_string(post->published ? sv("Published") : sv("Draft"))))
        return false;
    return blog_render(context, sv("posts/show"), status);
}

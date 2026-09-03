#include "comments_controller.h"

#include "../models/comment.h"
#include "../models/post.h"
#include "../models/user.h"
#include "../queries/blog_queries.h"
#include "concerns/blog_controller.h"

#include <ceasy/validation/validation.h>

void comments_create(Context *context) {
    User *user;
    Post *post = NULL;
    Comment comment = {0};
    ValidationErrors errors;
    String content;
    int64_t id;
    if (!require_authenticated_user(context) || !context_parse_form(context) ||
        !blog_id(context, &id) ||
        post_find(context, id, &post) != MODEL_RESULT_OK || post == NULL ||
        !post->published) {
        blog_error(context, sv("404 Not Found"),
                   sv("We couldn't find that page.\n"));
        return;
    }
    user = blog_current_user(context);
    content = normalized(context, context_form(context, sv("comment")), false);
    validation_errors_init(&errors, context->arena);
    if (!validation_length_between(string_as_view(&content), 1, 2000))
        validation_errors_add(
            &errors, sv("comment"),
            sv("Comment must be between 1 and 2000 characters."));
    if (validation_errors_any(&errors)) {
        view_set(context, sv("comment"), view_string(string_as_view(&content)));
        form_errors(context, &errors);
        if (!blog_render_post_show(context, post, user,
                                   sv("422 Unprocessable Content")))
            blog_error(context, sv("500 Internal Server Error"),
                       sv("Something went wrong.\n"));
        return;
    }
    comment.post_id = id;
    comment.user_id = user->id;
    comment.content = content;
    if (!comment_insert(context, &comment)) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    flash_set(context, sv("success"), sv("Comment added."));
    {
        String location = string_format_in(
            context->arena, "/posts/%lld#comments", (long long)id);
        context_redirect(context, string_as_view(&location));
    }
}

void comments_destroy(Context *context) {
    DatabaseStatement statement = {0};
    DatabaseStepResult step;
    int64_t id;
    User *user = blog_current_user(context);
    int64_t post_id;
    int64_t author_id;
    if (user == NULL ||
        !stringv_parse_int64(context_param(context, sv("id")), &id) ||
        id <= 0 ||
        !database_prepare(
            context->database, &statement,
            sv("SELECT post_id, user_id FROM comments WHERE id = ?")) ||
        !database_bind_int64(&statement, 1, id)) {
        database_statement_destroy(&statement);
        blog_error(context, sv("404 Not Found"),
                   sv("We couldn't find that page.\n"));
        return;
    }
    step = database_step(&statement);
    if (step != DATABASE_STEP_ROW) {
        database_statement_destroy(&statement);
        blog_error(context, sv("404 Not Found"),
                   sv("We couldn't find that page.\n"));
        return;
    }
    post_id = database_column_int64(&statement, 0);
    author_id = database_column_int64(&statement, 1);
    database_statement_destroy(&statement);
    if (!user->is_admin && user->id != author_id) {
        DatabaseStatement ps = {0};
        bool allowed =
            database_prepare(
                context->database, &ps,
                sv("SELECT 1 FROM posts WHERE id = ? AND user_id = ?")) &&
            database_bind_int64(&ps, 1, post_id) &&
            database_bind_int64(&ps, 2, user->id) &&
            database_step(&ps) == DATABASE_STEP_ROW;
        database_statement_destroy(&ps);
        if (!allowed) {
            blog_error(context, sv("403 Forbidden"),
                       sv("You don't have permission to do that.\n"));
            return;
        }
    }
    if (!database_prepare(context->database, &statement,
                          sv("DELETE FROM comments WHERE id = ?")) ||
        !database_bind_int64(&statement, 1, id) ||
        !database_execute(&statement)) {
        database_statement_destroy(&statement);
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    database_statement_destroy(&statement);
    flash_set(context, sv("success"), sv("Comment deleted."));
    {
        String location = string_format_in(
            context->arena, "/posts/%lld#comments", (long long)post_id);
        context_redirect(context, string_as_view(&location));
    }
}

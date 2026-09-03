#include "posts_controller.h"

#include "models/comment.h"
#include "models/user.h"
#include "queries/blog_queries.h"

#include <ceasy/security/password.h>
#include <ceasy/validation/validation.h>

#include <time.h>

static bool blog_prepare_view(Context *context);

static void blog_error(Context *context, StringView status,
                       StringView message) {
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

static bool blog_id(Context *context, int64_t *id) {
    return stringv_parse_int64(context_param(context, sv("id")), id) && *id > 0;
}

static User *blog_current_user(Context *context) {
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

static bool blog_render(Context *context, StringView path, StringView status) {
    if (!blog_prepare_view(context)) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return false;
    }
    return render_status(context, path, status);
}

static String normalized(Context *context, StringView value, bool lower) {
    String result = string_from_in(context->arena, stringv_trim(value));
    if (lower)
        string_lower(&result);
    return result;
}

static String blog_timestamp(Context *context) {
    time_t now = time(NULL);
    struct tm utc;
    char value[32];

    if (now == (time_t)-1 || gmtime_r(&now, &utc) == NULL ||
        strftime(value, sizeof(value), "%Y-%m-%d %H:%M:%S", &utc) == 0)
        return (String){0};
    return string_from_in(context->arena, stringv_from_cstr(value));
}

static void form_errors(Context *context, ValidationErrors *errors) {
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

static bool validate_registration(Context *context, StringView name,
                                  StringView email, StringView password,
                                  StringView confirmation,
                                  ValidationErrors *errors) {
    User *existing = NULL;
    if (!validation_present(name))
        validation_errors_add(errors, sv("name"), sv("Name is required."));
    else if (!validation_length_between(stringv_trim(name), 2, 80))
        validation_errors_add(errors, sv("name"),
                              sv("Name must be between 2 and 80 characters."));
    if (!validation_present(email) || !validation_email_like(email))
        validation_errors_add(errors, sv("email"), sv("Email is invalid."));
    else if (user_find_by_email(context, email, &existing) == MODEL_RESULT_OK)
        validation_errors_add(errors, sv("email"),
                              sv("Email is already registered."));
    if (!validation_length_between(password, 10, 1024))
        validation_errors_add(errors, sv("password"),
                              sv("Password must be at least 10 characters."));
    if (!validation_equal(password, confirmation))
        validation_errors_add(errors, sv("password_confirmation"),
                              sv("Password confirmation does not match."));
    return !validation_errors_any(errors);
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
void home_index(Context *context) { posts_index(context); }

static bool render_post_show(Context *context, Post *post, User *viewer,
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
        !render_post_show(context, post, viewer, sv("200 OK"))) {
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

void users_new(Context *context) {
    if (auth_signed_in(context)) {
        context_redirect(context, sv("/dashboard"));
        return;
    }
    view_set(context, sv("page_title"), view_string(sv("Create your account")));
    view_set(context, sv("name"), view_string((StringView){0}));
    view_set(context, sv("email"), view_string((StringView){0}));
    view_set(context, sv("csrf_token"), view_string(csrf_token(context)));
    blog_render(context, sv("users/new"), sv("200 OK"));
}
void users_create(Context *context) {
    ValidationErrors errors;
    String name, email, digest;
    StringView password, confirmation;
    User user = {0};
    if (!context_parse_form(context))
        return;
    name = normalized(context, context_form(context, sv("name")), false);
    email = normalized(context, context_form(context, sv("email")), true);
    password = context_form(context, sv("password"));
    confirmation = context_form(context, sv("password_confirmation"));
    validation_errors_init(&errors, context->arena);
    if (!validate_registration(context, string_as_view(&name),
                               string_as_view(&email), password, confirmation,
                               &errors)) {
        view_set(context, sv("page_title"),
                 view_string(sv("Create your account")));
        view_set(context, sv("name"), view_string(string_as_view(&name)));
        view_set(context, sv("email"), view_string(string_as_view(&email)));
        form_errors(context, &errors);
        view_set(context, sv("csrf_token"), view_string(csrf_token(context)));
        blog_render(context, sv("users/new"), sv("422 Unprocessable Content"));
        return;
    }
    if (!password_hash(context->arena, password, &digest)) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    user.name = name;
    user.email = email;
    user.password_digest = digest;
    user.bio = string_from_in(context->arena, (StringView){0});
    if (!user_insert(context, &user) || !auth_login(context, user.id)) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    flash_set(context, sv("success"), sv("Your account is ready."));
    context_redirect(context, sv("/dashboard"));
}

void sessions_new(Context *context) {
    if (auth_signed_in(context)) {
        context_redirect(context, sv("/dashboard"));
        return;
    }
    view_set(context, sv("page_title"), view_string(sv("Welcome back")));
    view_set(context, sv("email"), view_string((StringView){0}));
    view_set(context, sv("csrf_token"), view_string(csrf_token(context)));
    blog_render(context, sv("sessions/new"), sv("200 OK"));
}
void sessions_create(Context *context) {
    String email;
    StringView password;
    User *user = NULL;
    bool valid = false;
    ValidationErrors errors;
    if (!context_parse_form(context))
        return;
    email = normalized(context, context_form(context, sv("email")), true);
    password = context_form(context, sv("password"));
    validation_errors_init(&errors, context->arena);
    user_find_by_email(context, string_as_view(&email), &user);
    if (user != NULL)
        valid =
            password_verify(string_as_view(&user->password_digest), password);
    if (!valid) {
        validation_errors_add(&errors, sv("email"),
                              sv("Invalid email or password."));
        view_set(context, sv("page_title"), view_string(sv("Welcome back")));
        view_set(context, sv("email"), view_string(string_as_view(&email)));
        form_errors(context, &errors);
        view_set(context, sv("csrf_token"), view_string(csrf_token(context)));
        blog_render(context, sv("sessions/new"),
                    sv("422 Unprocessable Content"));
        return;
    }
    auth_login(context, user->id);
    flash_set(context, sv("success"), sv("Welcome back."));
    context_redirect(context, sv("/dashboard"));
}
void sessions_destroy(Context *context) {
    auth_logout(context);
    flash_set(context, sv("notice"), sv("You have been signed out."));
    context_redirect(context, sv("/"));
}

void dashboard_index(Context *context) {
    User *user = blog_current_user(context);
    PostArray posts = {0};
    if (user == NULL || !post_all_for_user(context, user->id, &posts) ||
        !view_set(context, sv("page_title"), view_string(sv("Dashboard"))) ||
        !view_set(context, sv("posts"), post_array_view(posts))) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    blog_render(context, sv("dashboard/index"), sv("200 OK"));
}
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
        if (!render_post_show(context, post, user,
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

void authors_show(Context *context) {
    int64_t id;
    User *user = NULL;
    PostArray posts = {0};
    if (!blog_id(context, &id) ||
        user_find(context, id, &user) != MODEL_RESULT_OK ||
        !post_all_published_for_user(context, id, &posts)) {
        blog_error(context, sv("404 Not Found"),
                   sv("We couldn't find that page.\n"));
        return;
    }
    view_set(context, sv("page_title"),
             view_string(string_as_view(&user->name)));
    view_set(context, sv("author"), user_view(user));
    view_set(context, sv("posts"), post_array_view(posts));
    blog_render(context, sv("authors/show"), sv("200 OK"));
}
void account_edit(Context *context) {
    User *user = blog_current_user(context);
    if (user == NULL)
        return;
    view_set(context, sv("page_title"), view_string(sv("Account settings")));
    view_set(context, sv("user"), user_view(user));
    blog_render(context, sv("account/edit"), sv("200 OK"));
}
void account_update(Context *context) {
    User *user = blog_current_user(context);
    String name, bio;
    ValidationErrors errors;
    if (user == NULL || !context_parse_form(context))
        return;
    name = normalized(context, context_form(context, sv("name")), false);
    bio = normalized(context, context_form(context, sv("bio")), false);
    validation_errors_init(&errors, context->arena);
    if (!validation_length_between(string_as_view(&name), 2, 80))
        validation_errors_add(&errors, sv("name"),
                              sv("Name must be between 2 and 80 characters."));
    if (!validation_length_at_most(string_as_view(&bio), 2000))
        validation_errors_add(&errors, sv("bio"),
                              sv("Bio must be at most 2000 characters."));
    if (validation_errors_any(&errors)) {
        user->name = name;
        user->bio = bio;
        view_set(context, sv("user"), user_view(user));
        form_errors(context, &errors);
        blog_render(context, sv("account/edit"),
                    sv("422 Unprocessable Content"));
        return;
    }
    user->name = name;
    user->bio = bio;
    if (!user_update(context, user)) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    flash_set(context, sv("success"), sv("Profile updated."));
    context_redirect(context, sv("/account"));
}
void account_password_update(Context *context) {
    User *user = blog_current_user(context);
    String digest;
    StringView current, next, confirmation;
    ValidationErrors errors;
    if (user == NULL || !context_parse_form(context))
        return;
    current = context_form(context, sv("current_password"));
    next = context_form(context, sv("password"));
    confirmation = context_form(context, sv("password_confirmation"));
    validation_errors_init(&errors, context->arena);
    if (!password_verify(string_as_view(&user->password_digest), current))
        validation_errors_add(&errors, sv("password"),
                              sv("Current password is incorrect."));
    if (!validation_length_between(next, 10, 1024))
        validation_errors_add(&errors, sv("password"),
                              sv("Password must be at least 10 characters."));
    if (!validation_equal(next, confirmation))
        validation_errors_add(&errors, sv("password_confirmation"),
                              sv("Password confirmation does not match."));
    if (validation_errors_any(&errors)) {
        view_set(context, sv("user"), user_view(user));
        form_errors(context, &errors);
        blog_render(context, sv("account/edit"),
                    sv("422 Unprocessable Content"));
        return;
    }
    if (!password_hash(context->arena, next, &digest)) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    user->password_digest = digest;
    if (!user_update(context, user) || !session_regenerate(context)) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    flash_set(context, sv("success"), sv("Password updated."));
    context_redirect(context, sv("/account"));
}
void health_index(Context *context) {
    context_send_text(context, sv("200 OK"), sv("ok\n"));
}
void ready_index(Context *context) {
    DatabaseStatement statement = {0};
    bool ok = database_prepare(context->database, &statement, sv("SELECT 1")) &&
              database_step(&statement) == DATABASE_STEP_ROW;
    database_statement_destroy(&statement);
    context_send_text(context,
                      ok ? sv("200 OK") : sv("503 Service Unavailable"),
                      ok ? sv("ready\n") : sv("not ready\n"));
}

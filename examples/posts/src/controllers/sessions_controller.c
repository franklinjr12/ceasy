#include "sessions_controller.h"

#include "../models/user.h"
#include "concerns/blog_controller.h"

#include <ceasy/security/password.h>
#include <ceasy/validation/validation.h>

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

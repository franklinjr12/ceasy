#include "account_controller.h"

#include "../models/user.h"
#include "concerns/blog_controller.h"

#include <ceasy/security/password.h>
#include <ceasy/validation/validation.h>

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

#include "users_controller.h"

#include "../models/user.h"
#include "concerns/blog_controller.h"

#include <ceasy/security/password.h>
#include <ceasy/validation/validation.h>

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

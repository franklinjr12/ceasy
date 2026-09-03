#include <ceasy/context.h>

#include <ceasy/ceasy.h>

static bool cookie_token(StringView value) {
    if (value.length == 0 || value.data == NULL) {
        return false;
    }
    for (size_t index = 0; index < value.length; index++) {
        char c = value.data[index];
        if (c <= 0x20 || c == ';' || c == '=' || c == '\r' || c == '\n') {
            return false;
        }
    }
    return true;
}

static bool cookie_parse(Context *context) {
    StringView remaining;

    if (context->cookies_parsed) {
        return context->cookie_parse_ok;
    }
    context->cookies_parsed = true;
    context->cookie_parse_ok = true;
    remaining = request_header(&context->request, sv("Cookie"));
    while (remaining.length > 0) {
        StringView pair;
        StringView next;
        StringView name;
        StringView value;

        if (stringv_split_once_char(remaining, ';', &pair, &next)) {
            remaining = next;
        } else {
            pair = remaining;
            remaining = (StringView){0};
        }
        pair = stringv_trim(pair);
        if (!stringv_split_once_char(pair, '=', &name, &value) ||
            !cookie_token(stringv_trim(name))) {
            continue;
        }
        name = stringv_trim(name);
        value = stringv_trim(value);
        if (context->cookie_count >= CEASY_MAX_REQUEST_COOKIES) {
            context->cookie_parse_ok = false;
            return false;
        }
        context->cookies[context->cookie_count++] =
            (CookieParam){.name = name, .value = value};
    }
    return true;
}

StringView context_cookie(Context *context, StringView name) {
    if (context == NULL || !cookie_parse(context)) {
        return (StringView){0};
    }
    for (size_t index = 0; index < context->cookie_count; index++) {
        if (stringv_equal(context->cookies[index].name, name)) {
            return context->cookies[index].value;
        }
    }
    return (StringView){0};
}

bool context_set_cookie(Context *context, StringView name, StringView value,
                        CookieOptions options) {
    String cookie;

    if (context == NULL || context->arena == NULL || !cookie_token(name) ||
        value.data == NULL || stringv_contains(value, sv("\r")) ||
        stringv_contains(value, sv("\n")) ||
        stringv_contains(options.path, sv("\r")) ||
        stringv_contains(options.path, sv("\n")) ||
        stringv_contains(options.domain, sv("\r")) ||
        stringv_contains(options.domain, sv("\n"))) {
        return false;
    }
    cookie = string_new_in(context->arena);
    if (!string_append(&cookie, name) || !string_append_char(&cookie, '=') ||
        !string_append(&cookie, value)) {
        return false;
    }
    if (options.path.length > 0 && (!string_append(&cookie, sv("; Path=")) ||
                                    !string_append(&cookie, options.path))) {
        return false;
    }
    if (options.domain.length > 0 &&
        (!string_append(&cookie, sv("; Domain=")) ||
         !string_append(&cookie, options.domain))) {
        return false;
    }
    if (options.max_age >= 0 &&
        !string_append_format(&cookie, "; Max-Age=%lld",
                              (long long)options.max_age)) {
        return false;
    }
    if (options.http_only && !string_append(&cookie, sv("; HttpOnly"))) {
        return false;
    }
    if (options.secure && !string_append(&cookie, sv("; Secure"))) {
        return false;
    }
    if (!string_append(&cookie, options.same_site == COOKIE_SAME_SITE_STRICT
                                    ? sv("; SameSite=Strict")
                                : options.same_site == COOKIE_SAME_SITE_NONE
                                    ? sv("; SameSite=None")
                                    : sv("; SameSite=Lax"))) {
        return false;
    }
    return context_add_header(context, sv("Set-Cookie"),
                              string_as_view(&cookie));
}

bool context_delete_cookie(Context *context, StringView name,
                           CookieOptions options) {
    options.max_age = 0;
    return context_set_cookie(context, name,
                              (StringView){.data = "", .length = 0}, options);
}

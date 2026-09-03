#ifndef CEASY_HTTP_COOKIE_H
#define CEASY_HTTP_COOKIE_H

#include <stdbool.h>
#include <stdint.h>

#include <ceasy/string/string.h>

typedef struct Context Context;

typedef struct {
    StringView name;
    StringView value;
} CookieParam;

typedef enum {
    COOKIE_SAME_SITE_LAX,
    COOKIE_SAME_SITE_STRICT,
    COOKIE_SAME_SITE_NONE
} CookieSameSite;

typedef struct {
    StringView path;
    StringView domain;
    int64_t max_age;
    bool http_only;
    bool secure;
    CookieSameSite same_site;
} CookieOptions;

StringView context_cookie(Context *context, StringView name);
bool context_set_cookie(Context *context, StringView name, StringView value,
                        CookieOptions options);
bool context_delete_cookie(Context *context, StringView name,
                           CookieOptions options);

#endif

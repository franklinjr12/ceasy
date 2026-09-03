#include <ceasy/ceasy.h>

#include <assert.h>

int main(void) {
    Arena arena;
    Context context = {0};
    CookieOptions options = {.path = sv("/"),
                             .max_age = 3600,
                             .http_only = true,
                             .same_site = COOKIE_SAME_SITE_STRICT};

    assert(arena_init(&arena, 4096));
    context.arena = &arena;
    context.request.headers[0] =
        (RequestHeader){.name = sv("Cookie"),
                        .value = sv("session=abc; empty=; spaced = yes; bad")};
    context.request.header_count = 1;
    assert(stringv_equal(context_cookie(&context, sv("session")), sv("abc")));
    assert(context_cookie(&context, sv("empty")).length == 0);
    assert(stringv_equal(context_cookie(&context, sv("spaced")), sv("yes")));
    assert(context_cookie(&context, sv("missing")).length == 0);
    assert(context_set_cookie(&context, sv("session"), sv("next"), options));
    assert(context_set_cookie(&context, sv("session"), sv("other"), options));
    assert(context.response_header_count == 2);
    assert(stringv_equal(context.response_headers[0].name, sv("Set-Cookie")));
    assert(stringv_contains(context.response_headers[0].value, sv("Path=/")));
    assert(stringv_contains(context.response_headers[0].value, sv("HttpOnly")));
    assert(stringv_contains(context.response_headers[0].value,
                            sv("SameSite=Strict")));
    assert(context_delete_cookie(&context, sv("session"), options));
    assert(
        stringv_contains(context.response_headers[2].value, sv("Max-Age=0")));
    assert(!context_set_cookie(&context, sv("bad\r\nname"), sv("x"), options));
    assert(
        !context_set_cookie(&context, sv("safe"), sv("bad\nvalue"), options));
    arena_destroy(&arena);
    return 0;
}

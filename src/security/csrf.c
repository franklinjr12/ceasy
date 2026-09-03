#include <ceasy/security/csrf.h>

#include <ceasy/context.h>
#include <ceasy/session/session.h>

#include <sodium.h>

#define CSRF_KEY "__ceasy.csrf.token"

StringView csrf_token(Context *context) {
    StringView token = {0};
    unsigned char bytes[32];
    char encoded[64];

    if (context == NULL || context->arena == NULL ||
        session_get_string(context, sv(CSRF_KEY), &token)) {
        return token;
    }
    if (sodium_init() < 0) {
        return (StringView){0};
    }
    randombytes_buf(bytes, sizeof(bytes));
    sodium_bin2base64(encoded, sizeof(encoded), bytes, sizeof(bytes),
                      sodium_base64_VARIANT_URLSAFE_NO_PADDING);
    if (!session_set_string(context, sv(CSRF_KEY),
                            stringv_from_cstr(encoded)) ||
        !session_get_string(context, sv(CSRF_KEY), &token)) {
        return (StringView){0};
    }
    return token;
}

bool csrf_verify(Context *context, StringView token) {
    StringView expected = csrf_token(context);

    return expected.length == token.length && expected.length > 0 &&
           sodium_init() >= 0 &&
           sodium_memcmp(expected.data, token.data, expected.length) == 0;
}

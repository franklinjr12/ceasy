#include <ceasy/flash/flash.h>

#include <ceasy/context.h>
#include <ceasy/session/session.h>

static bool flash_key(Context *context, StringView key, String *result) {
    if (context == NULL || context->arena == NULL || key.length == 0 ||
        stringv_contains(key, sv("."))) {
        return false;
    }
    *result = string_new_in(context->arena);
    return string_append(result, sv("__ceasy.flash.")) &&
           string_append(result, key);
}

bool flash_set(Context *context, StringView key, StringView message) {
    String full_key;

    return flash_key(context, key, &full_key) &&
           session_set_string(context, string_as_view(&full_key), message);
}

StringView flash_get(Context *context, StringView key) {
    String full_key;
    StringView message;
    String copy;

    if (!flash_key(context, key, &full_key) ||
        !session_get_string(context, string_as_view(&full_key), &message)) {
        return (StringView){0};
    }
    copy = string_from_in(context->arena, message);
    if (copy.data == NULL) {
        return (StringView){0};
    }
    session_delete(context, string_as_view(&full_key));
    return string_as_view(&copy);
}

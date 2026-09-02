#include "ceasy/context.h"

static bool context_hex_digit(char value, unsigned char *result) {
    if (value >= '0' && value <= '9') {
        *result = (unsigned char)(value - '0');
        return true;
    }
    if (value >= 'a' && value <= 'f') {
        *result = (unsigned char)(value - 'a' + 10);
        return true;
    }
    if (value >= 'A' && value <= 'F') {
        *result = (unsigned char)(value - 'A' + 10);
        return true;
    }
    return false;
}

static bool context_is_form_content_type(StringView value) {
    StringView prefix = sv("application/x-www-form-urlencoded");

    return value.length >= prefix.length &&
           stringv_equal_ignore_case(stringv_slice(value, 0, prefix.length),
                                     prefix);
}

static bool context_form_decode(Context *context, StringView source,
                                StringView *decoded) {
    bool needs_decode = false;

    for (size_t index = 0; index < source.length; index++) {
        if (source.data[index] == '+' || source.data[index] == '%') {
            needs_decode = true;
            break;
        }
    }

    if (!needs_decode) {
        *decoded = source;
        return true;
    }
    if (context->arena == NULL) {
        return false;
    }
    char *data = arena_alloc(context->arena, source.length + 1);
    if (data == NULL) {
        return false;
    }
    size_t output_length = 0;
    for (size_t index = 0; index < source.length; index++) {
        unsigned char high;
        unsigned char low;

        if (source.data[index] == '+') {
            data[output_length++] = ' ';
        } else if (source.data[index] == '%') {
            if (index + 2 >= source.length ||
                !context_hex_digit(source.data[index + 1], &high) ||
                !context_hex_digit(source.data[index + 2], &low)) {
                return false;
            }
            data[output_length++] = (char)((high << 4) | low);
            index += 2;
        } else {
            data[output_length++] = source.data[index];
        }
    }
    data[output_length] = '\0';
    *decoded = (StringView){.data = data, .length = output_length};
    return true;
}

bool context_parse_form(Context *context) {
    StringView remaining;

    if (context == NULL) {
        return false;
    }
    if (context->forms_parsed) {
        return context->form_parse_ok;
    }
    context->forms_parsed = true;
    context->form_parse_ok = true;
    if (!context_is_form_content_type(context->request.content_type)) {
        return true;
    }

    remaining = context->request.body;
    while (remaining.length > 0) {
        StringView pair;
        StringView name;
        StringView value;
        StringView decoded_name;
        StringView decoded_value;
        StringView next;

        if (stringv_split_once_char(remaining, '&', &pair, &next)) {
            remaining = next;
        } else {
            pair = remaining;
            remaining = (StringView){0};
        }
        if (!stringv_split_once_char(pair, '=', &name, &value)) {
            name = pair;
            value = (StringView){0};
        }
        if (context->form_count >= CEASY_MAX_FORM_PARAMS ||
            !context_form_decode(context, name, &decoded_name) ||
            !context_form_decode(context, value, &decoded_value)) {
            context->form_parse_ok = false;
            return false;
        }
        context->form_params[context->form_count++] =
            (FormParam){.name = decoded_name, .value = decoded_value};
    }
    return true;
}

StringView context_form(Context *context, StringView name) {
    if (context == NULL || !context_parse_form(context)) {
        return (StringView){0};
    }
    for (size_t index = 0; index < context->form_count; index++) {
        if (stringv_equal(context->form_params[index].name, name)) {
            return context->form_params[index].value;
        }
    }
    return (StringView){0};
}

StringView context_param(Context *context, StringView name) {
    if (context == NULL) {
        return (StringView){0};
    }
    for (size_t index = 0; index < context->route_param_count; index++) {
        if (stringv_equal(context->route_params[index].name, name)) {
            return context->route_params[index].value;
        }
    }
    return (StringView){0};
}

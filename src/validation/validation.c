#include <ceasy/validation/validation.h>

#include <ceasy/model/model.h>
#include <ceasy/view/view.h>

#include <ctype.h>
#include <string.h>

static const ModelField validation_fields[] = {
    {.name = sv("field"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(ValidationError, field)},
    {.name = sv("message"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(ValidationError, message)},
};

static const ModelDefinition validation_definition = {
    .name = sv("ValidationError"),
    .size = sizeof(ValidationError),
    .fields = validation_fields,
    .field_count = sizeof(validation_fields) / sizeof(validation_fields[0]),
};

void validation_errors_init(ValidationErrors *errors, Arena *arena) {
    if (errors != NULL) {
        memset(errors, 0, sizeof(*errors));
        errors->arena = arena;
    }
}

bool validation_errors_add(ValidationErrors *errors, StringView field,
                           StringView message) {
    ValidationError *replacement;
    size_t capacity;

    if (errors == NULL || errors->arena == NULL || field.data == NULL ||
        message.data == NULL) {
        return false;
    }
    if (errors->length == errors->capacity) {
        capacity = errors->capacity == 0 ? 4 : errors->capacity * 2;
        if (capacity < errors->capacity ||
            capacity > SIZE_MAX / sizeof(*replacement)) {
            return false;
        }
        replacement =
            arena_alloc(errors->arena, capacity * sizeof(*replacement));
        if (replacement == NULL) {
            return false;
        }
        if (errors->items != NULL && errors->length > 0) {
            memcpy(replacement, errors->items,
                   errors->length * sizeof(*replacement));
        }
        errors->items = replacement;
        errors->capacity = capacity;
    }
    errors->items[errors->length] =
        (ValidationError){.field = string_from_in(errors->arena, field),
                          .message = string_from_in(errors->arena, message)};
    if (errors->items[errors->length].field.data == NULL ||
        errors->items[errors->length].message.data == NULL) {
        return false;
    }
    errors->length++;
    return true;
}

bool validation_errors_any(const ValidationErrors *errors) {
    return errors != NULL && errors->length > 0;
}
size_t validation_errors_count(const ValidationErrors *errors) {
    return errors == NULL ? 0 : errors->length;
}
StringView validation_error_for(const ValidationErrors *errors,
                                StringView field) {
    if (errors == NULL) {
        return (StringView){0};
    }
    for (size_t index = 0; index < errors->length; index++) {
        if (stringv_equal(errors->items[index].field.data == NULL
                              ? (StringView){0}
                              : string_as_view(&errors->items[index].field),
                          field)) {
            return string_as_view(&errors->items[index].message);
        }
    }
    return (StringView){0};
}

bool validation_present(StringView value) {
    return stringv_trim(value).length > 0;
}
bool validation_length_between(StringView value, size_t minimum,
                               size_t maximum) {
    return value.length >= minimum && value.length <= maximum;
}
bool validation_length_at_most(StringView value, size_t maximum) {
    return value.length <= maximum;
}
bool validation_equal(StringView first, StringView second) {
    return stringv_equal(first, second);
}
bool validation_email_like(StringView value) {
    size_t at = SIZE_MAX;

    if (value.length == 0 || value.length > 254 || value.data == NULL) {
        return false;
    }
    for (size_t index = 0; index < value.length; index++) {
        if (isspace((unsigned char)value.data[index])) {
            return false;
        }
        if (value.data[index] == '@') {
            if (at != SIZE_MAX) {
                return false;
            }
            at = index;
        }
    }
    return at != SIZE_MAX && at > 0 && at + 1 < value.length;
}

ViewValue validation_errors_view(const ValidationErrors *errors) {
    return view_collection(errors == NULL ? NULL : errors->items,
                           errors == NULL ? 0 : errors->length,
                           &validation_definition);
}

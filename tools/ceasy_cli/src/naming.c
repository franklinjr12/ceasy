#include "naming.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void naming_error(char *error, size_t error_size, const char *format,
                         const char *value) {
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, format, value != NULL ? value : "");
    }
}

static bool naming_copy(char *destination, size_t capacity,
                        const char *source) {
    size_t length = source == NULL ? 0 : strlen(source);

    if (source == NULL || length + 1 > capacity) {
        return false;
    }
    memcpy(destination, source, length + 1);
    return true;
}

static bool naming_reserved(StringView value) {
    static const char *const words[] = {
        "auto",       "break",     "case",           "char",
        "const",      "continue",  "default",        "do",
        "double",     "else",      "enum",           "extern",
        "float",      "for",       "goto",           "if",
        "inline",     "int",       "long",           "register",
        "restrict",   "return",    "short",          "signed",
        "sizeof",     "static",    "struct",         "switch",
        "typedef",    "union",     "unsigned",       "void",
        "volatile",   "while",     "_Alignas",       "_Alignof",
        "_Atomic",    "_Bool",     "_Complex",       "_Generic",
        "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"};

    for (size_t index = 0; index < sizeof(words) / sizeof(words[0]); index++) {
        if (stringv_equal(value, stringv_from_cstr(words[index]))) {
            return true;
        }
    }
    return false;
}

static bool naming_field_valid(const char *name) {
    size_t length;

    if (name == NULL || name[0] < 'a' || name[0] > 'z') {
        return false;
    }
    length = strlen(name);
    if (length == 0 || name[length - 1] == '_') {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        if (!((name[index] >= 'a' && name[index] <= 'z') ||
              (name[index] >= '0' && name[index] <= '9') ||
              name[index] == '_')) {
            return false;
        }
    }
    return !naming_reserved(stringv_from_cstr(name));
}

static bool naming_model_valid(const char *name) {
    if (name == NULL || name[0] < 'A' || name[0] > 'Z') {
        return false;
    }
    for (size_t index = 0; name[index] != '\0'; index++) {
        if (!((name[index] >= 'A' && name[index] <= 'Z') ||
              (name[index] >= 'a' && name[index] <= 'z') ||
              (name[index] >= '0' && name[index] <= '9'))) {
            return false;
        }
    }
    return true;
}

static bool naming_snake(const char *source, char *destination,
                         size_t capacity) {
    size_t output = 0;

    for (size_t index = 0; source[index] != '\0'; index++) {
        unsigned char character = (unsigned char)source[index];
        bool boundary = index > 0 && isupper(character) &&
                        (islower((unsigned char)source[index - 1]) ||
                         (index + 1 < strlen(source) &&
                          islower((unsigned char)source[index + 1])));

        if (boundary) {
            if (output + 1 >= capacity) {
                return false;
            }
            destination[output++] = '_';
        }
        if (output + 1 >= capacity) {
            return false;
        }
        destination[output++] = (char)tolower(character);
    }
    destination[output] = '\0';
    return output > 0;
}

static bool naming_pluralize(const char *singular, char *plural,
                             size_t capacity) {
    size_t length = strlen(singular);
    bool es = length > 0 &&
              (singular[length - 1] == 's' || singular[length - 1] == 'x' ||
               singular[length - 1] == 'z' ||
               (length > 1 && singular[length - 2] == 'c' &&
                singular[length - 1] == 'h') ||
               (length > 1 && singular[length - 2] == 's' &&
                singular[length - 1] == 'h'));
    bool y_rule = length > 1 && singular[length - 1] == 'y' &&
                  strchr("aeiou", singular[length - 2]) == NULL;
    size_t suffix = es || y_rule ? 2 : 1;

    if (length + suffix + 1 > capacity) {
        return false;
    }
    memcpy(plural, singular, length + 1);
    if (y_rule) {
        plural[length - 1] = 'i';
        plural[length] = 'e';
        plural[length + 1] = 's';
    } else if (es) {
        plural[length] = 'e';
        plural[length + 1] = 's';
    } else {
        plural[length] = 's';
    }
    plural[length + suffix] = '\0';
    return true;
}

bool naming_model(const char *model_name, Naming *result, char *error,
                  size_t error_size) {
    if (result == NULL || !naming_model_valid(model_name)) {
        naming_error(error, error_size, "invalid model name '%s'", model_name);
        return false;
    }
    memset(result, 0, sizeof(*result));
    if (!naming_copy(result->type_name, sizeof(result->type_name),
                     model_name) ||
        !naming_snake(model_name, result->singular, sizeof(result->singular)) ||
        !naming_pluralize(result->singular, result->plural,
                          sizeof(result->plural)) ||
        !naming_copy(result->table_name, sizeof(result->table_name),
                     result->plural) ||
        !naming_copy(result->file_stem, sizeof(result->file_stem),
                     result->singular)) {
        naming_error(error, error_size, "model name '%s' is too long",
                     model_name);
        return false;
    }
    for (size_t index = 0; result->file_stem[index] != '\0'; index++) {
        if (index + 1 >= sizeof(result->include_guard)) {
            return false;
        }
        result->include_guard[index] =
            (char)toupper((unsigned char)result->file_stem[index]);
    }
    strncat(result->include_guard, "_MODEL_H",
            sizeof(result->include_guard) - strlen(result->include_guard) - 1);
    return true;
}

static bool naming_parse_type(const char *name, ModelFieldType *type,
                              char *canonical, size_t canonical_size) {
    if (strcmp(name, "string") == 0 || strcmp(name, "text") == 0) {
        *type = MODEL_FIELD_STRING;
        return naming_copy(canonical, canonical_size, name);
    }
    if (strcmp(name, "int64") == 0 || strcmp(name, "integer") == 0) {
        *type = MODEL_FIELD_INT64;
        return naming_copy(canonical, canonical_size, name);
    }
    if (strcmp(name, "bool") == 0) {
        *type = MODEL_FIELD_BOOL;
        return naming_copy(canonical, canonical_size, name);
    }
    return false;
}

bool naming_parse_fields(int argc, char **argv, FieldSpec *fields,
                         size_t *field_count, char *error, size_t error_size) {
    size_t count = 0;

    if (argc < 0 || field_count == NULL || fields == NULL ||
        (argc > 0 && argv == NULL)) {
        return false;
    }
    for (int argument = 0; argument < argc; argument++) {
        char *separator = strchr(argv[argument], ':');
        char field_name[CEASY_NAME_CAPACITY];
        char type_name[32];
        size_t name_length;
        size_t type_length;
        ModelFieldType type;

        if (separator == NULL || separator == argv[argument] ||
            separator[1] == '\0' || strchr(separator + 1, ':') != NULL) {
            naming_error(error, error_size, "invalid field specification '%s'",
                         argv[argument]);
            return false;
        }
        name_length = (size_t)(separator - argv[argument]);
        type_length = strlen(separator + 1);
        if (name_length + 1 > sizeof(field_name) ||
            type_length + 1 > sizeof(type_name)) {
            naming_error(error, error_size, "invalid field specification '%s'",
                         argv[argument]);
            return false;
        }
        memcpy(field_name, argv[argument], name_length);
        field_name[name_length] = '\0';
        memcpy(type_name, separator + 1, type_length + 1);
        if (!naming_field_valid(field_name)) {
            naming_error(error, error_size, "invalid field name '%s'",
                         field_name);
            return false;
        }
        if (strcmp(field_name, "id") == 0 ||
            strcmp(field_name, "created_at") == 0 ||
            strcmp(field_name, "updated_at") == 0) {
            naming_error(error, error_size, "reserved model field '%s'",
                         field_name);
            return false;
        }
        if (!naming_parse_type(type_name, &type, type_name,
                               sizeof(type_name))) {
            naming_error(error, error_size, "unsupported model field type '%s'",
                         type_name);
            return false;
        }
        for (size_t index = 0; index < count; index++) {
            if (strcmp(fields[index].name, field_name) == 0) {
                naming_error(error, error_size, "duplicate model field '%s'",
                             field_name);
                return false;
            }
        }
        if (count == CEASY_FIELD_CAPACITY) {
            naming_error(error, error_size, "too many model fields '%s'",
                         field_name);
            return false;
        }
        memset(&fields[count], 0, sizeof(fields[count]));
        naming_copy(fields[count].name, sizeof(fields[count].name), field_name);
        naming_copy(fields[count].type_name, sizeof(fields[count].type_name),
                    type_name);
        fields[count].type = type;
        count++;
    }
    *field_count = count;
    return true;
}

const char *naming_c_type(ModelFieldType type) {
    switch (type) {
    case MODEL_FIELD_STRING:
        return "String";
    case MODEL_FIELD_INT64:
        return "int64_t";
    case MODEL_FIELD_BOOL:
        return "bool";
    default:
        return "void";
    }
}

const char *naming_sql_type(ModelFieldType type) {
    return type == MODEL_FIELD_STRING ? "TEXT" : "INTEGER";
}

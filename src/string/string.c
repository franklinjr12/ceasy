#include "ceasy/string/string.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool stringv_valid(StringView value) {
    return value.length == 0 || value.data != NULL;
}

static bool stringv_ascii_space(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
           value == '\f' || value == '\v';
}

static unsigned char stringv_ascii_lower(unsigned char value) {
    if (value >= (unsigned char)'A' && value <= (unsigned char)'Z') {
        return (unsigned char)(value +
                               ((unsigned char)'a' - (unsigned char)'A'));
    }
    return value;
}

StringView stringv_from_cstr(const char *value) {
    if (value == NULL) {
        return (StringView){0};
    }
    return (StringView){.data = value, .length = strlen(value)};
}

bool stringv_empty(StringView value) { return value.length == 0; }

bool stringv_equal(StringView a, StringView b) {
    if (!stringv_valid(a) || !stringv_valid(b) || a.length != b.length) {
        return false;
    }
    return a.length == 0 || memcmp(a.data, b.data, a.length) == 0;
}

bool stringv_equal_ignore_case(StringView a, StringView b) {
    if (!stringv_valid(a) || !stringv_valid(b) || a.length != b.length) {
        return false;
    }
    for (size_t index = 0; index < a.length; index++) {
        if (stringv_ascii_lower((unsigned char)a.data[index]) !=
            stringv_ascii_lower((unsigned char)b.data[index])) {
            return false;
        }
    }
    return true;
}

int stringv_compare(StringView a, StringView b) {
    size_t common_length;
    int result;

    if (!stringv_valid(a) || !stringv_valid(b)) {
        return !stringv_valid(a) - !stringv_valid(b);
    }
    common_length = a.length < b.length ? a.length : b.length;
    result = common_length == 0 ? 0 : memcmp(a.data, b.data, common_length);
    if (result != 0) {
        return result < 0 ? -1 : 1;
    }
    if (a.length == b.length) {
        return 0;
    }
    return a.length < b.length ? -1 : 1;
}

bool stringv_starts_with(StringView value, StringView prefix) {
    if (!stringv_valid(value) || !stringv_valid(prefix) ||
        prefix.length > value.length) {
        return false;
    }
    return prefix.length == 0 ||
           memcmp(value.data, prefix.data, prefix.length) == 0;
}

bool stringv_ends_with(StringView value, StringView suffix) {
    if (!stringv_valid(value) || !stringv_valid(suffix) ||
        suffix.length > value.length) {
        return false;
    }
    return suffix.length == 0 ||
           memcmp(value.data + value.length - suffix.length, suffix.data,
                  suffix.length) == 0;
}

bool stringv_find(StringView value, StringView search, size_t *index) {
    if (index != NULL) {
        *index = SIZE_MAX;
    }
    if (!stringv_valid(value) || !stringv_valid(search) || index == NULL ||
        search.length > value.length) {
        return false;
    }
    if (search.length == 0) {
        *index = 0;
        return true;
    }
    for (size_t position = 0; position <= value.length - search.length;
         position++) {
        if (memcmp(value.data + position, search.data, search.length) == 0) {
            *index = position;
            return true;
        }
    }
    return false;
}

bool stringv_contains(StringView value, StringView search) {
    size_t index;

    return stringv_find(value, search, &index);
}

bool stringv_find_char(StringView value, char search, size_t *index) {
    if (index != NULL) {
        *index = SIZE_MAX;
    }
    if (!stringv_valid(value) || index == NULL) {
        return false;
    }
    for (size_t position = 0; position < value.length; position++) {
        if (value.data[position] == search) {
            *index = position;
            return true;
        }
    }
    return false;
}

bool stringv_rfind_char(StringView value, char search, size_t *index) {
    if (index != NULL) {
        *index = SIZE_MAX;
    }
    if (!stringv_valid(value) || index == NULL) {
        return false;
    }
    for (size_t position = value.length; position > 0; position--) {
        if (value.data[position - 1] == search) {
            *index = position - 1;
            return true;
        }
    }
    return false;
}

StringView stringv_slice(StringView value, size_t start, size_t length) {
    StringView result = {0};

    if (!stringv_valid(value) || start > value.length) {
        return result;
    }
    if (length > value.length - start) {
        length = value.length - start;
    }
    result.length = length;
    result.data = value.data == NULL ? NULL : value.data + start;
    return result;
}

StringView stringv_from(StringView value, size_t start) {
    if (start > value.length) {
        return (StringView){0};
    }
    return stringv_slice(value, start, value.length - start);
}

StringView stringv_trim_left(StringView value) {
    size_t start = 0;

    if (!stringv_valid(value)) {
        return (StringView){0};
    }
    while (start < value.length && stringv_ascii_space(value.data[start])) {
        start++;
    }
    return stringv_from(value, start);
}

StringView stringv_trim_right(StringView value) {
    size_t length = value.length;

    if (!stringv_valid(value)) {
        return (StringView){0};
    }
    while (length > 0 && stringv_ascii_space(value.data[length - 1])) {
        length--;
    }
    return stringv_slice(value, 0, length);
}

StringView stringv_trim(StringView value) {
    return stringv_trim_right(stringv_trim_left(value));
}

bool stringv_split_once(StringView value, StringView separator,
                        StringView *left, StringView *right) {
    size_t index;

    if (left != NULL) {
        *left = (StringView){0};
    }
    if (right != NULL) {
        *right = (StringView){0};
    }
    if (left == NULL || right == NULL || separator.length == 0 ||
        !stringv_find(value, separator, &index)) {
        return false;
    }
    *left = stringv_slice(value, 0, index);
    *right = stringv_from(value, index + separator.length);
    return true;
}

bool stringv_split_once_char(StringView value, char separator, StringView *left,
                             StringView *right) {
    size_t index;

    if (left != NULL) {
        *left = (StringView){0};
    }
    if (right != NULL) {
        *right = (StringView){0};
    }
    if (left == NULL || right == NULL ||
        !stringv_find_char(value, separator, &index)) {
        return false;
    }
    *left = stringv_slice(value, 0, index);
    *right = stringv_from(value, index + 1);
    return true;
}

static bool stringv_parse_unsigned(StringView value, uint64_t limit,
                                   bool allow_sign, bool *negative,
                                   uint64_t *result) {
    size_t index = 0;
    uint64_t parsed = 0;
    bool is_negative = false;

    if (!stringv_valid(value) || value.length == 0 || result == NULL) {
        return false;
    }
    if (allow_sign && (value.data[0] == '-' || value.data[0] == '+')) {
        is_negative = value.data[0] == '-';
        index = 1;
    }
    if (index == value.length || (is_negative && !allow_sign)) {
        return false;
    }
    for (; index < value.length; index++) {
        unsigned char digit = (unsigned char)value.data[index];

        if (digit < (unsigned char)'0' || digit > (unsigned char)'9') {
            return false;
        }
        digit = (unsigned char)(digit - (unsigned char)'0');
        if (parsed > (limit - digit) / 10) {
            return false;
        }
        parsed = parsed * 10 + digit;
    }
    if (negative != NULL) {
        *negative = is_negative;
    }
    *result = parsed;
    return true;
}

bool stringv_parse_int64(StringView value, int64_t *result) {
    bool negative;
    uint64_t parsed;
    uint64_t limit;

    if (result == NULL || !stringv_valid(value) || value.length == 0) {
        return false;
    }
    negative = value.data[0] == '-';
    limit = negative ? (uint64_t)INT64_MAX + 1u : (uint64_t)INT64_MAX;
    if (!stringv_parse_unsigned(value, limit, true, &negative, &parsed)) {
        return false;
    }
    if (negative) {
        *result =
            parsed == (uint64_t)INT64_MAX + 1u ? INT64_MIN : -(int64_t)parsed;
    } else {
        *result = (int64_t)parsed;
    }
    return true;
}

bool stringv_parse_int(StringView value, int *result) {
    int64_t parsed;

    if (result == NULL || !stringv_parse_int64(value, &parsed) ||
        parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }
    *result = (int)parsed;
    return true;
}

bool stringv_parse_size(StringView value, size_t *result) {
    uint64_t parsed;

    if (result == NULL || sizeof(size_t) > sizeof(uint64_t) ||
        !stringv_parse_unsigned(value, (uint64_t)SIZE_MAX, false, NULL,
                                &parsed)) {
        return false;
    }
    *result = (size_t)parsed;
    return true;
}

bool stringv_parse_bool(StringView value, bool *result) {
    if (result == NULL) {
        return false;
    }
    if (stringv_equal(value, sv("true")) || stringv_equal(value, sv("1"))) {
        *result = true;
        return true;
    }
    if (stringv_equal(value, sv("false")) || stringv_equal(value, sv("0"))) {
        *result = false;
        return true;
    }
    return false;
}

StringView stringv_remove_prefix(StringView value, StringView prefix) {
    return stringv_starts_with(value, prefix)
               ? stringv_from(value, prefix.length)
               : value;
}

StringView stringv_remove_suffix(StringView value, StringView suffix) {
    return stringv_ends_with(value, suffix)
               ? stringv_slice(value, 0, value.length - suffix.length)
               : value;
}

uint64_t stringv_hash(StringView value) {
    uint64_t hash = UINT64_C(14695981039346656037);

    if (!stringv_valid(value)) {
        return 0;
    }
    for (size_t index = 0; index < value.length; index++) {
        hash ^= (unsigned char)value.data[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool string_storage_offset(const String *string, const char *data,
                                  size_t *offset) {
    uintptr_t base;
    uintptr_t address;

    if (string == NULL || string->data == NULL || data == NULL ||
        string->capacity == SIZE_MAX) {
        return false;
    }
    base = (uintptr_t)(const void *)string->data;
    address = (uintptr_t)(const void *)data;
    if (address < base || address - base > string->capacity) {
        return false;
    }
    if (offset != NULL) {
        *offset = (size_t)(address - base);
    }
    return true;
}

static StringView string_rebase_view(const String *string, StringView value,
                                     bool inside, size_t offset) {
    if (!inside) {
        return value;
    }
    return (StringView){.data = string->data + offset, .length = value.length};
}

String string_new(Allocator allocator) {
    String string = {0};

    string.allocator = allocator;
    return string;
}

String string_new_in(Arena *arena) {
    return string_new(arena_allocator(arena));
}

String string_from(Allocator allocator, StringView value) {
    String string = string_new(allocator);

    if (!stringv_valid(value) || !string_reserve(&string, value.length)) {
        return string;
    }
    if (value.length != 0) {
        memcpy(string.data, value.data, value.length);
    }
    string.length = value.length;
    if (string.data != NULL) {
        string.data[string.length] = '\0';
    }
    return string;
}

String string_from_in(Arena *arena, StringView value) {
    return string_from(arena_allocator(arena), value);
}

String string_new_heap(void) { return string_new(allocator_heap()); }

String string_from_heap(StringView value) {
    return string_from(allocator_heap(), value);
}

StringView string_as_view(const String *string) {
    if (string == NULL || string->data == NULL) {
        return (StringView){0};
    }
    return (StringView){.data = string->data, .length = string->length};
}

const char *string_cstr(const String *string) {
    static const char empty[] = "";

    return string == NULL || string->data == NULL ? empty : string->data;
}

bool string_empty(const String *string) {
    return string == NULL || string->length == 0;
}

bool string_reserve(String *string, size_t capacity) {
    size_t new_capacity;
    size_t allocation_size;
    size_t old_size;
    char *new_data;

    if (string == NULL || string->length > string->capacity) {
        return false;
    }
    if (capacity <= string->capacity) {
        if (string->data != NULL) {
            string->data[string->length] = '\0';
        }
        return true;
    }
    new_capacity = string->capacity == 0 ? 16 : string->capacity;
    while (new_capacity < capacity) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = capacity;
            break;
        }
        new_capacity *= 2;
    }
    if (new_capacity == SIZE_MAX) {
        new_capacity = capacity;
    }
    if (new_capacity > SIZE_MAX - 1) {
        return false;
    }
    if (string->length == SIZE_MAX) {
        return false;
    }
    allocation_size = new_capacity + 1;
    old_size = string->data == NULL ? 0 : string->length + 1;
    if (string->allocator.realloc != NULL) {
        new_data = string->allocator.realloc(
            string->allocator.context, string->data, old_size, allocation_size);
    } else if (string->allocator.alloc != NULL) {
        new_data =
            string->allocator.alloc(string->allocator.context, allocation_size);
        if (new_data != NULL && string->data != NULL && old_size != 0) {
            memcpy(new_data, string->data, old_size);
        }
        if (new_data != NULL && string->data != NULL &&
            string->allocator.free != NULL) {
            string->allocator.free(string->allocator.context, string->data);
        }
    } else {
        new_data = NULL;
    }
    if (new_data == NULL) {
        return false;
    }
    string->data = new_data;
    string->capacity = new_capacity;
    string->data[string->length] = '\0';
    return true;
}

bool string_append(String *string, StringView value) {
    size_t required;
    size_t offset = 0;
    bool inside;

    if (string == NULL || !stringv_valid(value) ||
        value.length > SIZE_MAX - string->length) {
        return false;
    }
    if (value.length == 0) {
        return true;
    }
    required = string->length + value.length;
    inside = string_storage_offset(string, value.data, &offset);
    if (!string_reserve(string, required)) {
        return false;
    }
    value = string_rebase_view(string, value, inside, offset);
    memmove(string->data + string->length, value.data, value.length);
    string->length = required;
    string->data[string->length] = '\0';
    return true;
}

bool string_append_char(String *string, char value) {
    StringView view = {.data = &value, .length = 1};

    return string_append(string, view);
}

bool string_append_cstr(String *string, const char *value) {
    return string_append(string, stringv_from_cstr(value));
}

static bool string_append_format_v(String *string, const char *format,
                                   va_list arguments) {
    va_list measure_arguments;
    va_list write_arguments;
    int result;
    size_t old_length;
    size_t available;

    if (string == NULL || format == NULL) {
        return false;
    }
    va_copy(measure_arguments, arguments);
    result = vsnprintf(NULL, 0, format, measure_arguments);
    va_end(measure_arguments);
    if (result < 0) {
        return false;
    }
    old_length = string->length;
    if ((size_t)result > SIZE_MAX - old_length ||
        !string_reserve(string, old_length + (size_t)result)) {
        return false;
    }
    available = string->capacity - old_length + 1;
    va_copy(write_arguments, arguments);
    result = vsnprintf(string->data + old_length, available, format,
                       write_arguments);
    va_end(write_arguments);
    if (result < 0 || (size_t)result >= available) {
        string->data[old_length] = '\0';
        return false;
    }
    string->length = old_length + (size_t)result;
    return true;
}

bool string_append_format(String *string, const char *format, ...) {
    va_list arguments;
    bool result;

    va_start(arguments, format);
    result = string_append_format_v(string, format, arguments);
    va_end(arguments);
    return result;
}

static String string_format_v(Allocator allocator, const char *format,
                              va_list arguments) {
    String string = string_new(allocator);

    if (!string_append_format_v(&string, format, arguments)) {
        string_destroy(&string);
    }
    return string;
}

String string_format(Allocator allocator, const char *format, ...) {
    va_list arguments;
    String string;

    va_start(arguments, format);
    string = string_format_v(allocator, format, arguments);
    va_end(arguments);
    return string;
}

String string_format_in(Arena *arena, const char *format, ...) {
    va_list arguments;
    String string;

    va_start(arguments, format);
    string = string_format_v(arena_allocator(arena), format, arguments);
    va_end(arguments);
    return string;
}

bool string_prepend(String *string, StringView value) {
    size_t old_length;
    size_t offset = 0;
    bool inside;

    if (string == NULL || !stringv_valid(value) ||
        value.length > SIZE_MAX - string->length) {
        return false;
    }
    if (value.length == 0) {
        return true;
    }
    old_length = string->length;
    inside = string_storage_offset(string, value.data, &offset);
    if (!string_reserve(string, old_length + value.length)) {
        return false;
    }
    if (inside) {
        if (offset > SIZE_MAX - value.length) {
            return false;
        }
        value.data = string->data + offset + value.length;
    }
    memmove(string->data + value.length, string->data, old_length + 1);
    memmove(string->data, value.data, value.length);
    string->length = old_length + value.length;
    string->data[string->length] = '\0';
    return true;
}

bool string_copy(String *string, StringView value) {
    size_t offset = 0;
    bool inside;

    if (string == NULL || !stringv_valid(value)) {
        return false;
    }
    inside = string_storage_offset(string, value.data, &offset);
    if (!string_reserve(string, value.length)) {
        return false;
    }
    value = string_rebase_view(string, value, inside, offset);
    if (value.length != 0) {
        memmove(string->data, value.data, value.length);
    }
    string->length = value.length;
    string->data[string->length] = '\0';
    return true;
}

void string_clear(String *string) {
    if (string == NULL) {
        return;
    }
    string->length = 0;
    if (string->data != NULL) {
        string->data[0] = '\0';
    }
}

void string_upper(String *string) {
    if (string == NULL || string->data == NULL) {
        return;
    }
    for (size_t index = 0; index < string->length; index++) {
        if (string->data[index] >= 'a' && string->data[index] <= 'z') {
            string->data[index] = (char)(string->data[index] - 'a' + 'A');
        }
    }
}

void string_lower(String *string) {
    if (string == NULL || string->data == NULL) {
        return;
    }
    for (size_t index = 0; index < string->length; index++) {
        if (string->data[index] >= 'A' && string->data[index] <= 'Z') {
            string->data[index] = (char)(string->data[index] - 'A' + 'a');
        }
    }
}

bool string_remove(String *string, size_t start, size_t length) {
    size_t remaining;

    if (string == NULL || start > string->length) {
        return false;
    }
    remaining = string->length - start;
    if (length > remaining) {
        length = remaining;
    }
    if (string->data == NULL) {
        return length == 0;
    }
    memmove(string->data + start, string->data + start + length,
            remaining - length + 1);
    string->length -= length;
    return true;
}

static bool string_replace_build(String *string, StringView search,
                                 StringView replacement, bool replace_all) {
    StringView source;
    String result;
    size_t cursor = 0;
    bool found = false;

    if (search.length == 0 || !stringv_valid(search) ||
        !stringv_valid(replacement)) {
        return false;
    }
    source = string_as_view(string);
    result = string_new(string->allocator);
    while (cursor <= source.length) {
        StringView remaining = stringv_from(source, cursor);
        size_t relative;

        if (!stringv_find(remaining, search, &relative)) {
            if (!string_append(&result, remaining)) {
                string_destroy(&result);
                return false;
            }
            break;
        }
        size_t match = cursor + relative;

        if (!string_append(&result, stringv_slice(source, cursor, relative)) ||
            !string_append(&result, replacement)) {
            string_destroy(&result);
            return false;
        }
        found = true;
        cursor = match + search.length;
        if (!replace_all) {
            if (!string_append(&result, stringv_from(source, cursor))) {
                string_destroy(&result);
                return false;
            }
            break;
        }
    }
    if (!found) {
        string_destroy(&result);
        return true;
    }
    String old = *string;
    *string = result;
    string_destroy(&old);
    return true;
}

bool string_replace(String *string, StringView search, StringView replacement) {
    if (string == NULL) {
        return false;
    }
    return string_replace_build(string, search, replacement, false);
}

bool string_replace_all(String *string, StringView search,
                        StringView replacement) {
    if (string == NULL) {
        return false;
    }
    return string_replace_build(string, search, replacement, true);
}

void string_destroy(String *string) {
    if (string == NULL) {
        return;
    }
    if (string->data != NULL && string->allocator.free != NULL) {
        string->allocator.free(string->allocator.context, string->data);
    }
    memset(string, 0, sizeof(*string));
}

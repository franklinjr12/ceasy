#ifndef CEASY_STRING_STRING_H
#define CEASY_STRING_STRING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ceasy/memory/allocator.h>

typedef struct {
    const char *data;
    size_t length;
} StringView;

/* Creates a non-owning view of a string literal. Do not use on char * values.
 */
#define sv(value) ((StringView){.data = (value), .length = sizeof(value) - 1})

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    Allocator allocator;
} String;

/* StringView never owns data and is not necessarily null-terminated. */
StringView stringv_from_cstr(const char *value);
bool stringv_empty(StringView value);
bool stringv_equal(StringView a, StringView b);
bool stringv_equal_ignore_case(StringView a, StringView b);
int stringv_compare(StringView a, StringView b);
bool stringv_starts_with(StringView value, StringView prefix);
bool stringv_ends_with(StringView value, StringView suffix);
bool stringv_contains(StringView value, StringView search);
bool stringv_find(StringView value, StringView search, size_t *index);
bool stringv_find_char(StringView value, char search, size_t *index);
bool stringv_rfind_char(StringView value, char search, size_t *index);
StringView stringv_slice(StringView value, size_t start, size_t length);
StringView stringv_from(StringView value, size_t start);
StringView stringv_trim(StringView value);
StringView stringv_trim_left(StringView value);
StringView stringv_trim_right(StringView value);
bool stringv_split_once(StringView value, StringView separator,
                        StringView *left, StringView *right);
bool stringv_split_once_char(StringView value, char separator, StringView *left,
                             StringView *right);
bool stringv_parse_int(StringView value, int *result);
bool stringv_parse_int64(StringView value, int64_t *result);
bool stringv_parse_size(StringView value, size_t *result);
bool stringv_parse_bool(StringView value, bool *result);
StringView stringv_remove_prefix(StringView value, StringView prefix);
StringView stringv_remove_suffix(StringView value, StringView suffix);
uint64_t stringv_hash(StringView value);

/* String owns mutable, null-terminated storage through its allocator. */
String string_new(Allocator allocator);
String string_new_in(Arena *arena);
String string_from(Allocator allocator, StringView value);
String string_from_in(Arena *arena, StringView value);
String string_new_heap(void);
String string_from_heap(StringView value);
String string_format(Allocator allocator, const char *format, ...);
String string_format_in(Arena *arena, const char *format, ...);
StringView string_as_view(const String *string);
const char *string_cstr(const String *string);
bool string_empty(const String *string);
bool string_reserve(String *string, size_t capacity);
bool string_append(String *string, StringView value);
bool string_append_char(String *string, char value);
bool string_append_cstr(String *string, const char *value);
bool string_append_format(String *string, const char *format, ...);
bool string_prepend(String *string, StringView value);
bool string_copy(String *string, StringView value);
void string_clear(String *string);
void string_upper(String *string);
void string_lower(String *string);
bool string_replace(String *string, StringView search, StringView replacement);
bool string_replace_all(String *string, StringView search,
                        StringView replacement);
bool string_remove(String *string, size_t start, size_t length);
void string_destroy(String *string);

#endif

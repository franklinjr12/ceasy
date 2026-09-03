#ifndef CEASY_VALIDATION_H
#define CEASY_VALIDATION_H

#include <stdbool.h>
#include <stddef.h>

#include <ceasy/memory/arena.h>
#include <ceasy/string/string.h>

typedef struct {
    String field;
    String message;
} ValidationError;

typedef struct {
    Arena *arena;
    ValidationError *items;
    size_t length;
    size_t capacity;
} ValidationErrors;

void validation_errors_init(ValidationErrors *errors, Arena *arena);
bool validation_errors_add(ValidationErrors *errors, StringView field,
                           StringView message);
bool validation_errors_any(const ValidationErrors *errors);
size_t validation_errors_count(const ValidationErrors *errors);
StringView validation_error_for(const ValidationErrors *errors,
                                StringView field);
bool validation_present(StringView value);
bool validation_length_between(StringView value, size_t minimum,
                               size_t maximum);
bool validation_length_at_most(StringView value, size_t maximum);
bool validation_equal(StringView first, StringView second);
bool validation_email_like(StringView value);

struct ViewValue;
typedef struct ViewValue ViewValue;
ViewValue validation_errors_view(const ValidationErrors *errors);

#endif

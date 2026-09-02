#ifndef CEASY_VIEW_VIEW_H
#define CEASY_VIEW_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ceasy/model/model.h>
#include <ceasy/string/string.h>

typedef struct Context Context;

typedef enum {
    VIEW_VALUE_EMPTY = 0,
    VIEW_VALUE_STRING,
    VIEW_VALUE_INT64,
    VIEW_VALUE_BOOL,
    VIEW_VALUE_MODEL,
    VIEW_VALUE_COLLECTION
} ViewValueType;

typedef struct {
    const void *record;
    const ModelDefinition *definition;
} ViewModel;

typedef struct {
    const void *items;
    size_t length;
    const ModelDefinition *definition;
} ViewCollection;

typedef struct ViewValue {
    ViewValueType type;
    union {
        StringView string;
        int64_t int64;
        bool boolean;
        ViewModel model;
        ViewCollection collection;
    } as;
} ViewValue;

typedef struct {
    StringView name;
    ViewValue value;
} ViewEntry;

typedef struct {
    ViewEntry *entries;
    size_t length;
    size_t capacity;
} ViewData;

ViewValue view_string(StringView value);
ViewValue view_int64(int64_t value);
ViewValue view_bool(bool value);
ViewValue view_model(const void *record, const ModelDefinition *definition);
ViewValue view_collection(const void *items, size_t length,
                          const ModelDefinition *definition);

bool view_set(Context *context, StringView name, ViewValue value);
bool view_get(const ViewData *data, StringView name, ViewValue *value);
StringView view_last_error(const Context *context);

typedef enum {
    TEMPLATE_INSTRUCTION_TEXT = 0,
    TEMPLATE_INSTRUCTION_VARIABLE,
    TEMPLATE_INSTRUCTION_SECTION,
    TEMPLATE_INSTRUCTION_INVERSE_SECTION,
    TEMPLATE_INSTRUCTION_PARTIAL,
    TEMPLATE_INSTRUCTION_YIELD
} TemplateInstructionType;

typedef struct {
    TemplateInstructionType type;
    StringView value;
    size_t jump;
} TemplateInstruction;

typedef struct {
    const TemplateInstruction *instructions;
    size_t instruction_count;
    StringView source_name;
} TemplateProgram;

typedef struct {
    StringView source_name;
    size_t offset;
    size_t line;
    size_t column;
    String message;
} TemplateError;

bool template_parse(Arena *arena, StringView source, StringView source_name,
                    TemplateProgram *program, TemplateError *error);
void template_error_destroy(TemplateError *error);
bool view_path_valid(StringView path);

typedef struct {
    StringView path;
    StringView source_name;
    const TemplateInstruction *instructions;
    size_t instruction_count;
} EmbeddedView;

typedef struct {
    const EmbeddedView *views;
    size_t view_count;
} ViewBundle;

const EmbeddedView *view_bundle_find(const ViewBundle *bundle, StringView path);
const ViewBundle *ceasy_view_bundle(void);

/* Renders a view without applying the conventional application layout. */
bool view_render_to_string(Context *context, StringView path, String *output);
bool render(Context *context, StringView path);
bool render_without_layout(Context *context, StringView path);

#endif

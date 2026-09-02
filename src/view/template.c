#include "ceasy/view/view.h"

#include <string.h>

typedef struct {
    StringView name;
    size_t instruction_index;
} TemplateSection;

static void template_error_set(TemplateError *error, StringView source_name,
                               StringView source, size_t offset,
                               const char *message) {
    size_t line = 1;
    size_t column = 1;

    if (error == NULL) {
        return;
    }
    for (size_t index = 0; index < offset && index < source.length; index++) {
        if (source.data[index] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
    error->source_name = source_name;
    error->offset = offset;
    error->line = line;
    error->column = column;
    error->message = string_from_heap(stringv_from_cstr(message));
}

void template_error_destroy(TemplateError *error) {
    if (error == NULL) {
        return;
    }
    string_destroy(&error->message);
    memset(error, 0, sizeof(*error));
}

static bool template_identifier(StringView value) {
    if (value.length == 0 || value.data == NULL ||
        !((value.data[0] >= 'a' && value.data[0] <= 'z') ||
          (value.data[0] >= 'A' && value.data[0] <= 'Z') ||
          value.data[0] == '_')) {
        return false;
    }
    for (size_t index = 1; index < value.length; index++) {
        char character = value.data[index];
        if (!((character >= 'a' && character <= 'z') ||
              (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_')) {
            return false;
        }
    }
    return true;
}

static bool template_variable_path(StringView value) {
    StringView segment;
    size_t cursor = 0;

    if (value.length == 0 || value.data == NULL) {
        return false;
    }
    while (cursor < value.length) {
        size_t start = cursor;
        while (cursor < value.length && value.data[cursor] != '.') {
            cursor++;
        }
        segment = stringv_slice(value, start, cursor - start);
        if (!template_identifier(segment)) {
            return false;
        }
        if (cursor < value.length) {
            cursor++;
            if (cursor == value.length) {
                return false;
            }
        }
    }
    return true;
}

bool view_path_valid(StringView path) {
    size_t cursor = 0;

    if (path.length == 0 || path.data == NULL || path.data[0] == '/' ||
        path.data[0] == '\\' || (path.length >= 2 && path.data[1] == ':')) {
        return false;
    }
    while (cursor < path.length) {
        size_t start = cursor;
        while (cursor < path.length && path.data[cursor] != '/') {
            if (path.data[cursor] == '\\') {
                return false;
            }
            cursor++;
        }
        StringView segment = stringv_slice(path, start, cursor - start);
        if (segment.length == 0 || stringv_equal(segment, sv(".")) ||
            stringv_equal(segment, sv(".."))) {
            return false;
        }
        for (size_t index = 0; index < segment.length; index++) {
            char character = segment.data[index];
            if (!((character >= 'a' && character <= 'z') ||
                  (character >= 'A' && character <= 'Z') ||
                  (character >= '0' && character <= '9') || character == '_' ||
                  character == '-')) {
                return false;
            }
        }
        if (cursor < path.length) {
            cursor++;
            if (cursor == path.length) {
                return false;
            }
        }
    }
    return true;
}

static bool template_instruction_push(Arena *arena, TemplateInstruction **items,
                                      size_t *length, size_t *capacity,
                                      TemplateInstruction instruction) {
    if (*length == *capacity) {
        size_t new_capacity = *capacity == 0 ? 16 : *capacity * 2;
        TemplateInstruction *replacement;

        if (new_capacity < *capacity ||
            new_capacity > SIZE_MAX / sizeof(*replacement)) {
            return false;
        }
        replacement = arena_new_array(arena, TemplateInstruction, new_capacity);
        if (replacement == NULL) {
            return false;
        }
        if (*items != NULL && *length > 0) {
            memcpy(replacement, *items, *length * sizeof(*replacement));
        }
        *items = replacement;
        *capacity = new_capacity;
    }
    (*items)[(*length)++] = instruction;
    return true;
}

static bool template_section_push(Arena *arena, TemplateSection **sections,
                                  size_t *length, size_t *capacity,
                                  TemplateSection section) {
    if (*length == *capacity) {
        size_t new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        TemplateSection *replacement;

        if (new_capacity < *capacity ||
            new_capacity > SIZE_MAX / sizeof(*replacement)) {
            return false;
        }
        replacement = arena_new_array(arena, TemplateSection, new_capacity);
        if (replacement == NULL) {
            return false;
        }
        if (*sections != NULL && *length > 0) {
            memcpy(replacement, *sections, *length * sizeof(*replacement));
        }
        *sections = replacement;
        *capacity = new_capacity;
    }
    (*sections)[(*length)++] = section;
    return true;
}

static bool template_add_error(Arena *arena, TemplateError *error,
                               StringView source_name, StringView source,
                               size_t offset, const char *message) {
    (void)arena;
    template_error_set(error, source_name, source, offset, message);
    return false;
}

bool template_parse(Arena *arena, StringView source, StringView source_name,
                    TemplateProgram *program, TemplateError *error) {
    TemplateInstruction *instructions = NULL;
    TemplateSection *sections = NULL;
    size_t instruction_count = 0;
    size_t instruction_capacity = 0;
    size_t section_count = 0;
    size_t section_capacity = 0;
    size_t cursor = 0;

    if (program != NULL) {
        memset(program, 0, sizeof(*program));
    }
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
    }
    if (arena == NULL || program == NULL ||
        (source.length > 0 && source.data == NULL)) {
        return template_add_error(arena, error, source_name, source, 0,
                                  "invalid template input");
    }
    while (cursor < source.length) {
        size_t marker;
        StringView text;

        if (!stringv_find(stringv_from(source, cursor), sv("{{"), &marker)) {
            text = stringv_from(source, cursor);
            if (!template_instruction_push(
                    arena, &instructions, &instruction_count,
                    &instruction_capacity,
                    (TemplateInstruction){.type = TEMPLATE_INSTRUCTION_TEXT,
                                          .value = text})) {
                return template_add_error(arena, error, source_name, source,
                                          cursor, "out of memory");
            }
            cursor = source.length;
            continue;
        }
        marker += cursor;
        if (marker > cursor &&
            !template_instruction_push(
                arena, &instructions, &instruction_count, &instruction_capacity,
                (TemplateInstruction){
                    .type = TEMPLATE_INSTRUCTION_TEXT,
                    .value = stringv_slice(source, cursor, marker - cursor)})) {
            return template_add_error(arena, error, source_name, source, cursor,
                                      "out of memory");
        }
        size_t close_relative;
        if (!stringv_find(stringv_from(source, marker + 2), sv("}}"),
                          &close_relative)) {
            return template_add_error(arena, error, source_name, source, marker,
                                      "unclosed template tag");
        }
        size_t close = marker + 2 + close_relative;
        StringView tag =
            stringv_trim(stringv_slice(source, marker + 2, close - marker - 2));
        TemplateInstruction instruction = {0};

        if (tag.length == 0) {
            return template_add_error(arena, error, source_name, source, marker,
                                      "empty template tag");
        }
        if (stringv_equal(tag, sv("yield"))) {
            instruction.type = TEMPLATE_INSTRUCTION_YIELD;
        } else if (tag.data[0] == '#' || tag.data[0] == '^') {
            StringView name = stringv_trim(stringv_from(tag, 1));
            if (!template_variable_path(name)) {
                return template_add_error(arena, error, source_name, source,
                                          marker, "invalid section name");
            }
            instruction.type = tag.data[0] == '#'
                                   ? TEMPLATE_INSTRUCTION_SECTION
                                   : TEMPLATE_INSTRUCTION_INVERSE_SECTION;
            instruction.value = name;
        } else if (tag.data[0] == '/') {
            StringView name = stringv_trim(stringv_from(tag, 1));
            if (section_count == 0 || !template_variable_path(name)) {
                return template_add_error(arena, error, source_name, source,
                                          marker, "unexpected closing section");
            }
            if (!stringv_equal(name, sections[section_count - 1].name)) {
                return template_add_error(arena, error, source_name, source,
                                          marker, "mismatched closing section");
            }
            instructions[sections[section_count - 1].instruction_index].jump =
                instruction_count;
            section_count--;
        } else if (tag.data[0] == '>') {
            StringView name = stringv_trim(stringv_from(tag, 1));
            if (!view_path_valid(name)) {
                return template_add_error(arena, error, source_name, source,
                                          marker, "invalid partial path");
            }
            instruction.type = TEMPLATE_INSTRUCTION_PARTIAL;
            instruction.value = name;
        } else {
            if (!template_variable_path(tag)) {
                return template_add_error(arena, error, source_name, source,
                                          marker,
                                          "invalid template expression");
            }
            instruction.type = TEMPLATE_INSTRUCTION_VARIABLE;
            instruction.value = tag;
        }
        if (tag.data[0] != '/') {
            if (!template_instruction_push(
                    arena, &instructions, &instruction_count,
                    &instruction_capacity, instruction)) {
                return template_add_error(arena, error, source_name, source,
                                          marker, "out of memory");
            }
            if (instruction.type == TEMPLATE_INSTRUCTION_SECTION ||
                instruction.type == TEMPLATE_INSTRUCTION_INVERSE_SECTION) {
                if (!template_section_push(
                        arena, &sections, &section_count, &section_capacity,
                        (TemplateSection){.name = instruction.value,
                                          .instruction_index =
                                              instruction_count - 1})) {
                    return template_add_error(arena, error, source_name, source,
                                              marker, "out of memory");
                }
            }
        }
        cursor = close + 2;
    }
    if (section_count != 0) {
        return template_add_error(arena, error, source_name, source,
                                  source.length, "unclosed section");
    }
    program->instructions = instructions;
    program->instruction_count = instruction_count;
    program->source_name = source_name;
    return true;
}

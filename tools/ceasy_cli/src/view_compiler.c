#include "view_compiler.h"

#include <ceasy/ceasy.h>

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char relative[PATH_MAX];
    char logical[PATH_MAX];
    String source;
    Arena arena;
    TemplateProgram program;
} ViewSource;

typedef struct {
    ViewSource *items;
    size_t length;
    size_t capacity;
} ViewSources;

static bool compiler_append(String *output, const char *format, ...) {
    va_list arguments;
    va_list measure;
    int needed;
    char *buffer;
    bool success;

    va_start(arguments, format);
    va_copy(measure, arguments);
    needed = vsnprintf(NULL, 0, format, measure);
    va_end(measure);
    if (needed < 0) {
        va_end(arguments);
        return false;
    }
    buffer = malloc((size_t)needed + 1);
    if (buffer == NULL) {
        va_end(arguments);
        return false;
    }
    vsnprintf(buffer, (size_t)needed + 1, format, arguments);
    va_end(arguments);
    success = string_append(output, stringv_from_cstr(buffer));
    free(buffer);
    return success;
}

static void compiler_sources_destroy(ViewSources *sources) {
    if (sources == NULL) {
        return;
    }
    for (size_t index = 0; index < sources->length; index++) {
        arena_destroy(&sources->items[index].arena);
        string_destroy(&sources->items[index].source);
    }
    free(sources->items);
    memset(sources, 0, sizeof(*sources));
}

static bool compiler_read_file(const char *path, String *source) {
    FILE *input;
    long size;

    input = fopen(path, "rb");
    if (input == NULL || fseek(input, 0, SEEK_END) != 0 ||
        (size = ftell(input)) < 0 || fseek(input, 0, SEEK_SET) != 0 ||
        (unsigned long)size > SIZE_MAX - 1) {
        if (input != NULL) {
            fclose(input);
        }
        return false;
    }
    if (!string_reserve(source, (size_t)size)) {
        fclose(input);
        return false;
    }
    if (fread(source->data, 1, (size_t)size, input) != (size_t)size) {
        fclose(input);
        return false;
    }
    fclose(input);
    source->length = (size_t)size;
    if (source->data != NULL) {
        source->data[source->length] = '\0';
    }
    return true;
}

static bool compiler_logical_name(const char *relative, char *logical,
                                  size_t logical_size) {
    size_t length = strlen(relative);
    size_t output = 0;
    const char *cursor = relative;

    if (length < sizeof(".html") - 1 ||
        strcmp(relative + length - (sizeof(".html") - 1), ".html") != 0) {
        return false;
    }
    length -= sizeof(".html") - 1;
    while ((size_t)(cursor - relative) < length) {
        const char *segment_end = strchr(cursor, '/');
        size_t segment_length = segment_end == NULL
                                    ? length - (size_t)(cursor - relative)
                                    : (size_t)(segment_end - cursor);
        const char *name = cursor;

        if (segment_length > 0 && name[0] == '_') {
            name++;
            segment_length--;
        }
        if (output != 0) {
            if (output + 1 >= logical_size) {
                return false;
            }
            logical[output++] = '/';
        }
        if (output + segment_length >= logical_size) {
            return false;
        }
        memcpy(logical + output, name, segment_length);
        output += segment_length;
        if (segment_end == NULL) {
            break;
        }
        cursor = segment_end + 1;
    }
    logical[output] = '\0';
    return view_path_valid(stringv_from_cstr(logical));
}

static bool compiler_find(const ViewSources *sources, StringView logical,
                          size_t *index) {
    if (sources == NULL) {
        return false;
    }
    for (size_t position = 0; position < sources->length; position++) {
        if (stringv_equal(stringv_from_cstr(sources->items[position].logical),
                          logical)) {
            if (index != NULL) {
                *index = position;
            }
            return true;
        }
    }
    return false;
}

static bool compiler_add(ViewSources *sources, const char *root,
                         const char *relative) {
    ViewSource *source;
    char path[PATH_MAX];
    TemplateError error = {0};

    if (sources->length == sources->capacity) {
        size_t capacity = sources->capacity == 0 ? 16 : sources->capacity * 2;
        ViewSource *replacement;

        if (capacity < sources->capacity ||
            capacity > SIZE_MAX / sizeof(*replacement)) {
            return false;
        }
        replacement = realloc(sources->items, capacity * sizeof(*replacement));
        if (replacement == NULL) {
            return false;
        }
        sources->items = replacement;
        sources->capacity = capacity;
    }
    source = &sources->items[sources->length];
    memset(source, 0, sizeof(*source));
    if (snprintf(source->relative, sizeof(source->relative), "%s", relative) >=
            (int)sizeof(source->relative) ||
        !compiler_logical_name(source->relative, source->logical,
                               sizeof(source->logical)) ||
        snprintf(path, sizeof(path), "%s/views/%s", root, relative) >=
            (int)sizeof(path)) {
        return false;
    }
    source->source = string_new_heap();
    sources->length++;
    if (!compiler_read_file(path, &source->source) ||
        !arena_init(&source->arena, 4096) ||
        !template_parse(&source->arena, string_as_view(&source->source),
                        stringv_from_cstr(source->relative), &source->program,
                        &error)) {
        fprintf(stderr, "error: %s", source->relative);
        if (error.message.data != NULL) {
            fprintf(stderr, ":%zu:%zu: %.*s", error.line, error.column,
                    (int)error.message.length, error.message.data);
        }
        fputc('\n', stderr);
        template_error_destroy(&error);
        arena_destroy(&source->arena);
        string_destroy(&source->source);
        sources->length--;
        return false;
    }
    template_error_destroy(&error);
    return true;
}

static bool compiler_scan(ViewSources *sources, const char *root,
                          const char *relative) {
    char directory[PATH_MAX];
    DIR *entries;
    struct dirent *entry;
    bool success = true;

    if (snprintf(directory, sizeof(directory), "%s/views%s%s", root,
                 relative[0] == '\0' ? "" : "/",
                 relative) >= (int)sizeof(directory)) {
        return false;
    }
    entries = opendir(directory);
    if (entries == NULL) {
        fprintf(stderr, "error: cannot scan %s: %s\n", directory,
                strerror(errno));
        return false;
    }
    while (success && (entry = readdir(entries)) != NULL) {
        char child[PATH_MAX];
        char full[PATH_MAX];
        struct stat status;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (snprintf(child, sizeof(child), "%s%s%s", relative,
                     relative[0] == '\0' ? "" : "/",
                     entry->d_name) >= (int)sizeof(child) ||
            snprintf(full, sizeof(full), "%s/views/%s", root, child) >=
                (int)sizeof(full) ||
            stat(full, &status) != 0) {
            success = false;
            break;
        }
        if (S_ISDIR(status.st_mode)) {
            success = compiler_scan(sources, root, child);
        } else if (S_ISREG(status.st_mode) &&
                   stringv_ends_with(stringv_from_cstr(entry->d_name),
                                     sv(".html"))) {
            success = compiler_add(sources, root, child);
        }
    }
    closedir(entries);
    return success;
}

static int compiler_source_compare(const void *left, const void *right) {
    const ViewSource *a = left;
    const ViewSource *b = right;

    return strcmp(a->logical, b->logical);
}

static bool compiler_validate_partials(const ViewSources *sources) {
    for (size_t index = 0; index < sources->length; index++) {
        const TemplateProgram *program = &sources->items[index].program;
        for (size_t instruction = 0; instruction < program->instruction_count;
             instruction++) {
            const TemplateInstruction *item =
                &program->instructions[instruction];
            if (item->type == TEMPLATE_INSTRUCTION_PARTIAL &&
                !compiler_find(sources, item->value, NULL)) {
                fprintf(stderr, "error: %s: partial '%.*s' not found\n",
                        sources->items[index].relative, (int)item->value.length,
                        item->value.data);
                return false;
            }
        }
    }
    return true;
}

static bool compiler_visit(const ViewSources *sources, size_t index,
                           unsigned char *state) {
    const TemplateProgram *program = &sources->items[index].program;

    state[index] = 1;
    for (size_t instruction = 0; instruction < program->instruction_count;
         instruction++) {
        const TemplateInstruction *item = &program->instructions[instruction];
        size_t target;

        if (item->type != TEMPLATE_INSTRUCTION_PARTIAL ||
            !compiler_find(sources, item->value, &target)) {
            continue;
        }
        if (state[target] == 1) {
            fprintf(stderr, "error: partial cycle involving '%s'\n",
                    sources->items[target].logical);
            return false;
        }
        if (state[target] == 0 && !compiler_visit(sources, target, state)) {
            return false;
        }
    }
    state[index] = 2;
    return true;
}

static bool compiler_validate_cycles(const ViewSources *sources) {
    unsigned char *state = calloc(sources->length, sizeof(*state));
    bool success = state != NULL || sources->length == 0;

    if (!success) {
        return false;
    }
    for (size_t index = 0; success && index < sources->length; index++) {
        if (state[index] == 0) {
            success = compiler_visit(sources, index, state);
        }
    }
    free(state);
    return success;
}

static bool compiler_c_string(String *output, StringView value) {
    if (!string_append_char(output, '"')) {
        return false;
    }
    for (size_t index = 0; index < value.length; index++) {
        unsigned char byte = (unsigned char)value.data[index];
        if (byte == '"' || byte == '\\') {
            if (!string_append_char(output, '\\') ||
                !string_append_char(output, (char)byte)) {
                return false;
            }
        } else if (byte == '\n') {
            if (!string_append(output, sv("\\n"))) {
                return false;
            }
        } else if (byte == '\r') {
            if (!string_append(output, sv("\\r"))) {
                return false;
            }
        } else if (byte == '\t') {
            if (!string_append(output, sv("\\t"))) {
                return false;
            }
        } else if (byte < 32 || byte == 127) {
            if (!string_append_format(output, "\\%03o", byte)) {
                return false;
            }
        } else if (!string_append_char(output, (char)byte)) {
            return false;
        }
    }
    return string_append_char(output, '"');
}

static const char *compiler_instruction_name(TemplateInstructionType type) {
    switch (type) {
    case TEMPLATE_INSTRUCTION_TEXT:
        return "TEMPLATE_INSTRUCTION_TEXT";
    case TEMPLATE_INSTRUCTION_VARIABLE:
        return "TEMPLATE_INSTRUCTION_VARIABLE";
    case TEMPLATE_INSTRUCTION_SECTION:
        return "TEMPLATE_INSTRUCTION_SECTION";
    case TEMPLATE_INSTRUCTION_INVERSE_SECTION:
        return "TEMPLATE_INSTRUCTION_INVERSE_SECTION";
    case TEMPLATE_INSTRUCTION_PARTIAL:
        return "TEMPLATE_INSTRUCTION_PARTIAL";
    case TEMPLATE_INSTRUCTION_YIELD:
        return "TEMPLATE_INSTRUCTION_YIELD";
    default:
        return "TEMPLATE_INSTRUCTION_TEXT";
    }
}

static bool compiler_generate(String *output, const ViewSources *sources) {
    if (!string_append(
            output, sv("/*\n * Generated by Ceasy.\n *\n * Source: HTML files "
                       "under views/\n *\n * Do not edit this file manually.\n "
                       "*/\n\n#include <ceasy/view/view.h>\n\n"))) {
        return false;
    }
    for (size_t source_index = 0; source_index < sources->length;
         source_index++) {
        const TemplateProgram *program = &sources->items[source_index].program;
        if (program->instruction_count == 0) {
            continue;
        }
        if (!compiler_append(output,
                             "static const TemplateInstruction "
                             "ceasy_view_%zu[] = {\n",
                             source_index)) {
            return false;
        }
        for (size_t index = 0; index < program->instruction_count; index++) {
            const TemplateInstruction *instruction =
                &program->instructions[index];
            if (!compiler_append(
                    output, "    {.type = %s, .value = {.data = ",
                    compiler_instruction_name(instruction->type)) ||
                !compiler_c_string(output, instruction->value) ||
                !compiler_append(output, ", .length = %zu}, .jump = %zu},\n",
                                 instruction->value.length,
                                 instruction->jump)) {
                return false;
            }
        }
        if (!string_append(output, sv("};\n\n"))) {
            return false;
        }
    }
    if (sources->length == 0) {
        if (!string_append(output,
                           sv("static const EmbeddedView ceasy_views[1] = "
                              "{{0}};\n\n"))) {
            return false;
        }
    } else if (!string_append(
                   output,
                   sv("static const EmbeddedView ceasy_views[] = {\n"))) {
        return false;
    }
    for (size_t index = 0; index < sources->length; index++) {
        const TemplateProgram *program = &sources->items[index].program;
        char instruction_name[64];

        if (program->instruction_count == 0) {
            strcpy(instruction_name, "NULL");
        } else if (snprintf(instruction_name, sizeof(instruction_name),
                            "ceasy_view_%zu",
                            index) >= (int)sizeof(instruction_name)) {
            return false;
        }
        if (!compiler_append(output, "    {.path = {.data = ") ||
            !compiler_c_string(
                output, stringv_from_cstr(sources->items[index].logical)) ||
            !compiler_append(output,
                             ", .length = %zu}, .source_name = {.data = ",
                             strlen(sources->items[index].logical)) ||
            !compiler_c_string(
                output, stringv_from_cstr(sources->items[index].relative)) ||
            !compiler_append(output,
                             ", .length = %zu}, .instructions = %s, "
                             ".instruction_count = %zu},\n",
                             strlen(sources->items[index].relative),
                             instruction_name, program->instruction_count)) {
            return false;
        }
    }
    if (sources->length > 0 && !string_append(output, sv("};\n\n"))) {
        return false;
    }
    return compiler_append(output,
                           "static const ViewBundle ceasy_bundle = "
                           "{.views = ceasy_views, .view_count = %zu};\n\n"
                           "const ViewBundle *ceasy_view_bundle(void) {\n"
                           "    return &ceasy_bundle;\n}\n",
                           sources->length);
}

static bool compiler_write(const CeasyProject *project, const String *output) {
    char directory[PATH_MAX];
    char target[PATH_MAX];
    char temporary[PATH_MAX];
    FILE *file;
    bool success;
    bool close_success;

    if (!project_path(project, "src/generated/ceasy_views.c", target,
                      sizeof(target)) ||
        !project_path(project, "src/generated", directory, sizeof(directory))) {
        return false;
    }
    if (mkdir(directory, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "error: cannot create %s: %s\n", directory,
                strerror(errno));
        return false;
    }
    if (snprintf(temporary, sizeof(temporary), "%s.tmp-%ld", target,
                 (long)getpid()) >= (int)sizeof(temporary)) {
        return false;
    }
    file = fopen(temporary, "wb");
    if (file == NULL) {
        return false;
    }
    success = fwrite(output->data, 1, output->length, file) == output->length &&
              fflush(file) == 0;
    close_success = fclose(file) == 0;
    success = success && close_success;
    if (!success) {
        unlink(temporary);
        return false;
    }
    if (rename(temporary, target) != 0) {
        unlink(temporary);
        return false;
    }
    printf("compile src/generated/ceasy_views.c\n");
    return true;
}

bool view_compiler_run(const CeasyProject *project) {
    ViewSources sources = {0};
    String output = string_new_heap();
    bool success =
        project != NULL && compiler_scan(&sources, project->root, "");

    if (success) {
        qsort(sources.items, sources.length, sizeof(sources.items[0]),
              compiler_source_compare);
        for (size_t index = 1; success && index < sources.length; index++) {
            if (strcmp(sources.items[index - 1].logical,
                       sources.items[index].logical) == 0) {
                fprintf(stderr, "error: duplicate logical view '%s'\n",
                        sources.items[index].logical);
                success = false;
            }
        }
    }
    success = success && compiler_validate_partials(&sources) &&
              compiler_validate_cycles(&sources) &&
              compiler_generate(&output, &sources) &&
              compiler_write(project, &output);
    string_destroy(&output);
    compiler_sources_destroy(&sources);
    return success;
}

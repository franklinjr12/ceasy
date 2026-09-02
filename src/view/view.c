#include "ceasy/ceasy.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define CEASY_VIEW_MAX_NESTING 64

typedef struct {
    const void *record;
    const ModelDefinition *definition;
} ViewScope;

typedef enum {
    VIEW_LOAD_MISSING = 0,
    VIEW_LOAD_FOUND,
    VIEW_LOAD_ERROR
} ViewLoadResult;

static void view_set_error(Context *context, StringView message) {
    if (context == NULL) {
        return;
    }
    context->view_error = message;
}

static bool view_error_text(Context *context, StringView source_name,
                            size_t line, size_t column, StringView message) {
    String error;

    if (context == NULL || context->arena == NULL) {
        return false;
    }
    if (source_name.length > (size_t)INT_MAX ||
        message.length > (size_t)INT_MAX) {
        return false;
    }
    error = string_format_in(context->arena, "%.*s:%zu:%zu: %.*s",
                             (int)source_name.length, source_name.data, line,
                             column, (int)message.length, message.data);
    if (error.data == NULL) {
        return false;
    }
    view_set_error(context, string_as_view(&error));
    return true;
}

static bool view_simple_error(Context *context, const char *message) {
    String error;

    if (context == NULL || context->arena == NULL) {
        return false;
    }
    error = string_from_in(context->arena, stringv_from_cstr(message));
    if (error.data == NULL) {
        return false;
    }
    view_set_error(context, string_as_view(&error));
    return true;
}

ViewValue view_string(StringView value) {
    return (ViewValue){.type = VIEW_VALUE_STRING, .as.string = value};
}

ViewValue view_int64(int64_t value) {
    return (ViewValue){.type = VIEW_VALUE_INT64, .as.int64 = value};
}

ViewValue view_bool(bool value) {
    return (ViewValue){.type = VIEW_VALUE_BOOL, .as.boolean = value};
}

ViewValue view_model(const void *record, const ModelDefinition *definition) {
    return (ViewValue){
        .type = VIEW_VALUE_MODEL,
        .as.model = {.record = record, .definition = definition}};
}

ViewValue view_collection(const void *items, size_t length,
                          const ModelDefinition *definition) {
    return (ViewValue){.type = VIEW_VALUE_COLLECTION,
                       .as.collection = {.items = items,
                                         .length = length,
                                         .definition = definition}};
}

static bool view_name_valid(StringView name) {
    if (name.length == 0 || name.data == NULL) {
        return false;
    }
    for (size_t index = 0; index < name.length; index++) {
        char character = name.data[index];
        if (!((character >= 'a' && character <= 'z') ||
              (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_' ||
              character == '.')) {
            return false;
        }
    }
    return true;
}

bool view_set(Context *context, StringView name, ViewValue value) {
    if (context == NULL || context->arena == NULL || !view_name_valid(name)) {
        return false;
    }
    if (context->view_data.values.allocator.alloc == NULL &&
        !sm_init_in(&context->view_data.values, context->arena)) {
        return false;
    }
    return sm_set_value(
        &context->view_data.values, name,
        sm_object(&context->view_data.values, &value, sizeof(value)));
}

bool view_get(const ViewData *data, StringView name, ViewValue *value) {
    const ViewValue *stored;

    if (data == NULL || value == NULL || !view_name_valid(name)) {
        return false;
    }
    stored = sm_get_object_const(&data->values, name, sizeof(*stored));
    if (stored == NULL) {
        return false;
    }
    *value = *stored;
    return true;
}

StringView view_last_error(const Context *context) {
    return context == NULL ? (StringView){0} : context->view_error;
}

const EmbeddedView *view_bundle_find(const ViewBundle *bundle,
                                     StringView path) {
    if (bundle == NULL || (bundle->view_count > 0 && bundle->views == NULL)) {
        return NULL;
    }
    for (size_t index = 0; index < bundle->view_count; index++) {
        if (stringv_equal(bundle->views[index].path, path)) {
            return &bundle->views[index];
        }
    }
    return NULL;
}

static ViewLoadResult view_load_filesystem(Context *context, StringView path,
                                           TemplateProgram *program,
                                           TemplateError *error) {
    char root[PATH_MAX];
    char filename[PATH_MAX];
    char partial_filename[PATH_MAX];
    FILE *input;
    long file_size;
    char *source;
    size_t slash;

    if (!view_path_valid(path) || path.length > (size_t)INT_MAX ||
        getcwd(root, sizeof(root)) == NULL ||
        snprintf(filename, sizeof(filename), "%s/views/%.*s.html", root,
                 (int)path.length, path.data) >= (int)sizeof(filename)) {
        view_simple_error(context, "invalid view path");
        return VIEW_LOAD_ERROR;
    }
    input = fopen(filename, "rb");
    if (input == NULL) {
        if (errno == ENOENT || errno == ENOTDIR) {
            if (!stringv_rfind_char(path, '/', &slash)) {
                if (snprintf(partial_filename, sizeof(partial_filename),
                             "%s/views/_%.*s.html", root, (int)path.length,
                             path.data) >= (int)sizeof(partial_filename)) {
                    view_simple_error(context, "invalid view path");
                    return VIEW_LOAD_ERROR;
                }
            } else if (snprintf(partial_filename, sizeof(partial_filename),
                                "%s/views/%.*s_%.*s.html", root,
                                (int)(slash + 1), path.data,
                                (int)(path.length - slash - 1),
                                path.data + slash + 1) >=
                       (int)sizeof(partial_filename)) {
                view_simple_error(context, "invalid view path");
                return VIEW_LOAD_ERROR;
            }
            input = fopen(partial_filename, "rb");
            if (input == NULL && (errno == ENOENT || errno == ENOTDIR)) {
                return VIEW_LOAD_MISSING;
            }
            if (input == NULL) {
                view_simple_error(context, "could not open view file");
                return VIEW_LOAD_ERROR;
            }
            if (snprintf(filename, sizeof(filename), "%s", partial_filename) >=
                (int)sizeof(filename)) {
                fclose(input);
                view_simple_error(context, "invalid view path");
                return VIEW_LOAD_ERROR;
            }
        }
        if (input == NULL) {
            view_simple_error(context, "could not open view file");
            return VIEW_LOAD_ERROR;
        }
    }
    if (fseek(input, 0, SEEK_END) != 0 || (file_size = ftell(input)) < 0 ||
        fseek(input, 0, SEEK_SET) != 0 ||
        (unsigned long)file_size > SIZE_MAX - 1) {
        fclose(input);
        view_simple_error(context, "could not determine view file size");
        return VIEW_LOAD_ERROR;
    }
    source = arena_alloc(context->arena, (size_t)file_size + 1);
    if (source == NULL ||
        fread(source, 1, (size_t)file_size, input) != (size_t)file_size) {
        fclose(input);
        view_simple_error(context, "could not read view file");
        return VIEW_LOAD_ERROR;
    }
    fclose(input);
    source[file_size] = '\0';
    StringView source_name = stringv_from_cstr(filename);
    if (!template_parse(
            context->arena,
            (StringView){.data = source, .length = (size_t)file_size},
            source_name, program, error)) {
        if (error != NULL) {
            view_error_text(context, error->source_name, error->line,
                            error->column, string_as_view(&error->message));
        }
        return VIEW_LOAD_ERROR;
    }
    program->source_name = path;
    return VIEW_LOAD_FOUND;
}

static ViewLoadResult view_load(Context *context, StringView path,
                                TemplateProgram *program,
                                TemplateError *error) {
    ViewLoadResult filesystem;
    const ViewBundle *bundle;
    const EmbeddedView *embedded;

    if (context == NULL || context->arena == NULL || program == NULL ||
        !view_path_valid(path)) {
        view_simple_error(context, "invalid view path");
        return VIEW_LOAD_ERROR;
    }
    filesystem = view_load_filesystem(context, path, program, error);
    if (filesystem != VIEW_LOAD_MISSING) {
        return filesystem;
    }
    bundle = ceasy_view_bundle();
    embedded = view_bundle_find(bundle, path);
    if (embedded == NULL) {
        if (path.length > (size_t)INT_MAX) {
            view_simple_error(context, "view not found");
        } else {
            String message =
                string_format_in(context->arena, "view not found: %.*s",
                                 (int)path.length, path.data);
            if (message.data != NULL) {
                view_set_error(context, string_as_view(&message));
            }
        }
        return VIEW_LOAD_MISSING;
    }
    program->instructions = embedded->instructions;
    program->instruction_count = embedded->instruction_count;
    program->source_name = embedded->source_name.length == 0
                               ? embedded->path
                               : embedded->source_name;
    return VIEW_LOAD_FOUND;
}

static const ModelField *view_find_field(const ModelDefinition *definition,
                                         StringView name) {
    if (definition == NULL || definition->fields == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < definition->field_count; index++) {
        if (stringv_equal(definition->fields[index].name, name)) {
            return &definition->fields[index];
        }
    }
    return NULL;
}

static bool view_model_field(const ViewModel *model, StringView field_name,
                             ViewValue *value) {
    const ModelField *field;
    size_t field_size;
    const unsigned char *address;

    if (model == NULL || model->record == NULL || model->definition == NULL ||
        value == NULL) {
        return false;
    }
    field = view_find_field(model->definition, field_name);
    if (field == NULL) {
        return false;
    }
    field_size = field->type == MODEL_FIELD_STRING  ? sizeof(String)
                 : field->type == MODEL_FIELD_INT64 ? sizeof(int64_t)
                 : field->type == MODEL_FIELD_BOOL  ? sizeof(bool)
                                                    : 0;
    if (field_size == 0 || field->offset > model->definition->size ||
        field_size > model->definition->size - field->offset) {
        return false;
    }
    address = (const unsigned char *)model->record + field->offset;
    if (field->type == MODEL_FIELD_STRING) {
        *value = view_string(string_as_view((const String *)address));
    } else if (field->type == MODEL_FIELD_INT64) {
        *value = view_int64(*(const int64_t *)address);
    } else {
        *value = view_bool(*(const bool *)address);
    }
    return true;
}

static bool view_resolve(Context *context, StringView path,
                         const ViewScope *scope, ViewValue *value) {
    size_t dot;
    StringView root_name = path;

    if (stringv_find_char(path, '.', &dot)) {
        root_name = stringv_slice(path, 0, dot);
    }
    if (scope != NULL && scope->record != NULL && scope->definition != NULL &&
        !stringv_find_char(path, '.', &dot) &&
        view_model_field(&(ViewModel){.record = scope->record,
                                      .definition = scope->definition},
                         path, value)) {
        return true;
    }
    if (!view_get(&context->view_data, root_name, value)) {
        return false;
    }
    if (!stringv_find_char(path, '.', &dot)) {
        return true;
    }
    StringView field_name = stringv_from(path, dot + 1);
    if (field_name.length == 0 || stringv_find_char(field_name, '.', &dot)) {
        return false;
    }
    if (value->type != VIEW_VALUE_MODEL ||
        !view_model_field(&value->as.model, field_name, value)) {
        return false;
    }
    return true;
}

static bool view_truthy(ViewValue value) {
    switch (value.type) {
    case VIEW_VALUE_STRING:
        return value.as.string.length > 0;
    case VIEW_VALUE_INT64:
        return value.as.int64 != 0;
    case VIEW_VALUE_BOOL:
        return value.as.boolean;
    case VIEW_VALUE_MODEL:
        return value.as.model.record != NULL;
    case VIEW_VALUE_COLLECTION:
        return value.as.collection.length > 0;
    default:
        return false;
    }
}

static bool view_render_value(String *output, ViewValue value) {
    switch (value.type) {
    case VIEW_VALUE_EMPTY:
        return true;
    case VIEW_VALUE_STRING:
        return html_escape_append(output, value.as.string);
    case VIEW_VALUE_INT64:
        return string_append_format(output, "%lld", (long long)value.as.int64);
    case VIEW_VALUE_BOOL:
        return string_append(output,
                             value.as.boolean ? sv("true") : sv("false"));
    default:
        return false;
    }
}

static bool view_render_program(Context *context,
                                const TemplateProgram *program, size_t start,
                                size_t end, const ViewScope *scope,
                                String *output, StringView yield_html,
                                bool allow_yield, size_t depth);

static bool view_render_partial(Context *context, StringView path,
                                const ViewScope *scope, String *output,
                                size_t depth) {
    TemplateProgram program = {0};
    TemplateError error = {0};
    ViewLoadResult result;

    if (depth >= CEASY_VIEW_MAX_NESTING) {
        view_simple_error(context, "template nesting depth exceeded");
        return false;
    }
    result = view_load(context, path, &program, &error);
    template_error_destroy(&error);
    if (result != VIEW_LOAD_FOUND) {
        return false;
    }
    return view_render_program(context, &program, 0, program.instruction_count,
                               scope, output, (StringView){0}, false,
                               depth + 1);
}

static bool view_render_program(Context *context,
                                const TemplateProgram *program, size_t start,
                                size_t end, const ViewScope *scope,
                                String *output, StringView yield_html,
                                bool allow_yield, size_t depth) {
    if (depth > CEASY_VIEW_MAX_NESTING || program == NULL || output == NULL ||
        (program->instruction_count > 0 && program->instructions == NULL) ||
        end > program->instruction_count || start > end) {
        view_simple_error(context, "invalid template program");
        return false;
    }
    for (size_t index = start; index < end; index++) {
        const TemplateInstruction *instruction = &program->instructions[index];
        if (instruction->type == TEMPLATE_INSTRUCTION_TEXT) {
            if (!string_append(output, instruction->value)) {
                view_simple_error(context,
                                  "out of memory while rendering view");
                return false;
            }
        } else if (instruction->type == TEMPLATE_INSTRUCTION_VARIABLE) {
            ViewValue value;
            if (!view_resolve(context, instruction->value, scope, &value)) {
                String message = string_format_in(
                    context->arena, "unknown value '%.*s'",
                    (int)instruction->value.length, instruction->value.data);
                if (message.data != NULL) {
                    view_set_error(context, string_as_view(&message));
                }
                return false;
            }
            if (!view_render_value(output, value)) {
                view_simple_error(context, "value cannot be rendered directly");
                return false;
            }
        } else if (instruction->type == TEMPLATE_INSTRUCTION_PARTIAL) {
            if (!view_render_partial(context, instruction->value, scope, output,
                                     depth)) {
                return false;
            }
        } else if (instruction->type == TEMPLATE_INSTRUCTION_YIELD) {
            if (!allow_yield || yield_html.data == NULL) {
                view_simple_error(context,
                                  "yield is only available in a layout");
                return false;
            }
            if (!string_append(output, yield_html)) {
                view_simple_error(context,
                                  "out of memory while rendering yield");
                return false;
            }
        } else {
            ViewValue value;
            bool truthy;
            bool inverse =
                instruction->type == TEMPLATE_INSTRUCTION_INVERSE_SECTION;

            if (!view_resolve(context, instruction->value, scope, &value)) {
                String message = string_format_in(
                    context->arena, "unknown value '%.*s'",
                    (int)instruction->value.length, instruction->value.data);
                if (message.data != NULL) {
                    view_set_error(context, string_as_view(&message));
                }
                return false;
            }
            truthy = view_truthy(value);
            if (inverse) {
                truthy = !truthy;
            }
            if (instruction->jump < index + 1 || instruction->jump > end) {
                view_simple_error(context, "invalid section jump");
                return false;
            }
            if (truthy && value.type == VIEW_VALUE_COLLECTION && !inverse) {
                if (value.as.collection.definition == NULL ||
                    value.as.collection.definition->size == 0 ||
                    (value.as.collection.length > 0 &&
                     value.as.collection.items == NULL)) {
                    view_simple_error(context, "invalid view collection");
                    return false;
                }
                for (size_t item_index = 0;
                     item_index < value.as.collection.length; item_index++) {
                    if (item_index >
                        SIZE_MAX / value.as.collection.definition->size) {
                        view_simple_error(context,
                                          "invalid view collection size");
                        return false;
                    }
                    ViewScope item_scope = {
                        .record =
                            (const unsigned char *)value.as.collection.items +
                            item_index * value.as.collection.definition->size,
                        .definition = value.as.collection.definition};
                    if (!view_render_program(context, program, index + 1,
                                             instruction->jump, &item_scope,
                                             output, yield_html, allow_yield,
                                             depth)) {
                        return false;
                    }
                }
            } else if (truthy &&
                       !view_render_program(context, program, index + 1,
                                            instruction->jump, scope, output,
                                            yield_html, allow_yield, depth)) {
                return false;
            }
            index = instruction->jump - 1;
        }
    }
    return true;
}

bool view_render_to_string(Context *context, StringView path, String *output) {
    TemplateProgram program = {0};
    TemplateError error = {0};
    ViewLoadResult result;

    if (context == NULL || context->arena == NULL || output == NULL) {
        return false;
    }
    result = view_load(context, path, &program, &error);
    template_error_destroy(&error);
    if (result != VIEW_LOAD_FOUND) {
        return false;
    }
    return view_render_program(context, &program, 0, program.instruction_count,
                               NULL, output, (StringView){0}, false, 0);
}

static bool view_render_with_layout(Context *context, StringView path,
                                    String *output) {
    TemplateProgram body_program = {0};
    TemplateProgram layout_program = {0};
    TemplateError error = {0};
    ViewLoadResult body_result;
    ViewLoadResult layout_result;
    String body = string_new_in(context->arena);

    body_result = view_load(context, path, &body_program, &error);
    template_error_destroy(&error);
    if (body_result != VIEW_LOAD_FOUND ||
        !view_render_program(context, &body_program, 0,
                             body_program.instruction_count, NULL, &body,
                             (StringView){0}, false, 0)) {
        string_destroy(&body);
        return false;
    }
    layout_result =
        view_load(context, sv("layouts/application"), &layout_program, &error);
    template_error_destroy(&error);
    if (layout_result == VIEW_LOAD_MISSING) {
        if (!string_append(output, string_as_view(&body))) {
            view_simple_error(context, "out of memory while rendering view");
            string_destroy(&body);
            return false;
        }
        string_destroy(&body);
        return true;
    }
    if (layout_result != VIEW_LOAD_FOUND) {
        string_destroy(&body);
        return false;
    }
    bool success = view_render_program(context, &layout_program, 0,
                                       layout_program.instruction_count, NULL,
                                       output, string_as_view(&body), true, 0);
    string_destroy(&body);
    return success;
}

bool render_without_layout(Context *context, StringView path) {
    String output;
    bool success;

    if (context == NULL || context->arena == NULL) {
        return false;
    }
    output = string_new_in(context->arena);
    success = view_render_to_string(context, path, &output);
    if (!success) {
        context_send_text(context, sv("500 Internal Server Error"),
                          view_last_error(context));
        return false;
    }
    return context_send_html(context, sv("200 OK"), string_as_view(&output));
}

bool render(Context *context, StringView path) {
    String output;

    if (context == NULL || context->arena == NULL) {
        return false;
    }
    output = string_new_in(context->arena);
    if (!view_render_with_layout(context, path, &output)) {
        context_send_text(context, sv("500 Internal Server Error"),
                          view_last_error(context));
        return false;
    }
    return context_send_html(context, sv("200 OK"), string_as_view(&output));
}

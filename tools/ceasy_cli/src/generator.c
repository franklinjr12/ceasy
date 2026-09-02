#include "generator.h"

#include "naming.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char relative[PATH_MAX];
    String content;
    char path[PATH_MAX];
} GeneratedFile;

typedef struct {
    char path[PATH_MAX];
} GeneratedDirectory;

static bool generated_append(String *string, const char *format, ...) {
    va_list arguments;
    int needed;
    va_list measure;
    char *buffer;

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
    bool result = string_append(string, stringv_from_cstr(buffer));
    free(buffer);
    return result;
}

static bool generated_timestamp(char *timestamp, size_t size) {
    time_t now = time(NULL);
    struct tm local;

    return now != (time_t)-1 && localtime_r(&now, &local) != NULL &&
           strftime(timestamp, size, "%Y%m%d%H%M%S", &local) == 14;
}

static bool generated_directory(const char *path, GeneratedDirectory *created,
                                size_t *created_count) {
    int result;

    if (*created_count >= 64 || strlen(path) >= sizeof(created[0].path)) {
        return false;
    }
    result = mkdir(path, 0755);
    if (result == 0) {
        strcpy(created[*created_count].path, path);
        (*created_count)++;
        return true;
    }
    return errno == EEXIST;
}

static bool generated_parent_dirs(const char *path, GeneratedDirectory *created,
                                  size_t *created_count) {
    char parent[PATH_MAX];
    char *last;

    if (strlen(path) + 1 > sizeof(parent)) {
        return false;
    }
    strcpy(parent, path);
    last = strrchr(parent, '/');
    if (last == NULL) {
        return true;
    }
    *last = '\0';
    for (char *cursor = parent + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\0';
            if (!generated_directory(parent, created, created_count)) {
                *cursor = '/';
                return false;
            }
            *cursor = '/';
        }
    }
    return generated_directory(parent, created, created_count);
}

static void generated_cleanup(GeneratedFile *files, size_t written,
                              GeneratedDirectory *directories,
                              size_t directory_count) {
    for (size_t index = 0; index < written; index++) {
        unlink(files[index].path);
    }
    for (size_t index = directory_count; index > 0; index--) {
        rmdir(directories[index - 1].path);
    }
}

static bool generator_write_files(const CeasyProject *project,
                                  GeneratedFile *files, size_t count) {
    size_t written = 0;
    char relatives[16][PATH_MAX];
    GeneratedDirectory directories[64] = {0};
    size_t directory_count = 0;

    if (count > 16) {
        return false;
    }
    for (size_t index = 0; index < count; index++) {
        if (strlen(files[index].relative) >= sizeof(relatives[index])) {
            fprintf(stderr, "error: generated path is too long\n");
            return false;
        }
        strcpy(relatives[index], files[index].relative);
    }

    for (size_t index = 0; index < count; index++) {
        if (files[index].content.data == NULL) {
            fprintf(stderr, "error: could not prepare generated file %s\n",
                    relatives[index]);
            return false;
        }
        if (!project_path(project, relatives[index], files[index].path,
                          sizeof(files[index].path))) {
            fprintf(stderr, "error: generated path is too long\n");
            return false;
        }
        if (access(files[index].path, F_OK) == 0) {
            fprintf(stderr, "error: %s already exists\n",
                    files[index].relative);
            return false;
        }
    }
    for (size_t index = 0; index < count; index++) {
        FILE *output;
        bool write_ok;

        if (!generated_parent_dirs(files[index].path, directories,
                                   &directory_count)) {
            fprintf(stderr, "error: cannot create directory for %s\n",
                    relatives[index]);
            generated_cleanup(files, written, directories, directory_count);
            return false;
        }
        output = fopen(files[index].path, "wb");
        write_ok = output != NULL;
        if (write_ok) {
            write_ok = fwrite(files[index].content.data, 1,
                              files[index].content.length,
                              output) == files[index].content.length;
            write_ok = fclose(output) == 0 && write_ok;
        }
        if (!write_ok) {
            fprintf(stderr, "error: cannot write %s\n", relatives[index]);
            unlink(files[index].path);
            generated_cleanup(files, written, directories, directory_count);
            return false;
        }
        printf("create %s\n", relatives[index]);
        written++;
    }
    return true;
}

static void generated_destroy(GeneratedFile *files, size_t count) {
    for (size_t index = 0; index < count; index++) {
        string_destroy(&files[index].content);
    }
}

static bool generated_model_header(String *out, const Naming *n,
                                   const FieldSpec *fields, size_t count) {
    if (!generated_append(
            out,
            "#ifndef %s\n#define %s\n\n#include <ceasy/ceasy.h>\n\ntypedef "
            "struct {\n    int64_t id;\n",
            n->include_guard, n->include_guard)) {
        return false;
    }
    for (size_t index = 0; index < count; index++) {
        if (!generated_append(out, "    %s %s;\n",
                              naming_c_type(fields[index].type),
                              fields[index].name)) {
            return false;
        }
    }
    return generated_append(
        out,
        "    String created_at;\n    String updated_at;\n} %s;\n\ntypedef "
        "struct {\n    %s *items;\n    size_t length;\n} "
        "%sArray;\n\nModelResult %s_find(Context *context, int64_t id, %s "
        "**post);\nbool %s_all(Context *context, %sArray *posts);\nbool "
        "%s_insert(Context *context, %s *post);\nModelResult %s_update(Context "
        "*context, %s *post);\nModelResult %s_destroy(Context *context, %s "
        "*post);\n\n#endif\n",
        n->type_name, n->type_name, n->type_name, n->singular, n->type_name,
        n->singular, n->type_name, n->singular, n->type_name, n->singular,
        n->type_name, n->singular, n->type_name);
}

static bool generated_columns(String *out, const FieldSpec *fields,
                              size_t count) {
    if (!string_append(out, sv("id"))) {
        return false;
    }
    for (size_t index = 0; index < count; index++) {
        if (!generated_append(out, ", %s", fields[index].name)) {
            return false;
        }
    }
    return generated_append(out, ", created_at, updated_at");
}

static bool generated_model_source(String *out, const Naming *n,
                                   const FieldSpec *fields, size_t count) {
    if (!generated_append(out,
                          "#include \"%s.h\"\n\n#include <stddef.h>\n\nstatic "
                          "const ModelField %s_fields[] = {\n",
                          n->file_stem, n->singular)) {
        return false;
    }
    if (!generated_append(out,
                          "    {.name = sv(\"id\"), .type = MODEL_FIELD_INT64, "
                          ".offset = offsetof(%s, id), .primary_key = true, "
                          ".insertable = false, .updatable = false},\n",
                          n->type_name)) {
        return false;
    }
    for (size_t index = 0; index < count; index++) {
        const char *type =
            fields[index].type == MODEL_FIELD_STRING ? "MODEL_FIELD_STRING"
            : fields[index].type == MODEL_FIELD_BOOL ? "MODEL_FIELD_BOOL"
                                                     : "MODEL_FIELD_INT64";
        if (!generated_append(out,
                              "    {.name = sv(\"%s\"), .type = %s, .offset = "
                              "offsetof(%s, %s), .primary_key = false, "
                              ".insertable = true, .updatable = true},\n",
                              fields[index].name, type, n->type_name,
                              fields[index].name)) {
            return false;
        }
    }
    if (!generated_append(
            out,
            "    {.name = sv(\"created_at\"), .type = MODEL_FIELD_STRING, "
            ".offset = offsetof(%s, created_at), .primary_key = false, "
            ".insertable = false, .updatable = false},\n    {.name = "
            "sv(\"updated_at\"), .type = MODEL_FIELD_STRING, .offset = "
            "offsetof(%s, updated_at), .primary_key = false, .insertable = "
            "false, .updatable = false},\n};\n\nstatic const ModelDefinition "
            "%s_definition = {\n    .name = sv(\"%s\"),\n    .table_name = "
            "sv(\"%s\"),\n    .size = sizeof(%s),\n    .fields = %s_fields,\n  "
            "  .field_count = sizeof(%s_fields) / sizeof(%s_fields[0]),\n    "
            ".find_sql = sv(\"SELECT ",
            n->type_name, n->type_name, n->singular, n->type_name, n->plural,
            n->type_name, n->singular, n->singular, n->singular)) {
        return false;
    }
    if (!generated_columns(out, fields, count) ||
        !generated_append(
            out, " FROM %s WHERE id = ?\"),\n    .all_sql = sv(\"SELECT ",
            n->plural) ||
        !generated_columns(out, fields, count) ||
        !generated_append(out,
                          " FROM %s ORDER BY id\"),\n    .insert_sql = sv(\"",
                          n->plural)) {
        return false;
    }
    if (count == 0) {
        if (!generated_append(out, "INSERT INTO %s DEFAULT VALUES\"),\n",
                              n->plural)) {
            return false;
        }
    } else {
        if (!generated_append(out, "INSERT INTO %s (", n->plural)) {
            return false;
        }
        for (size_t index = 0; index < count; index++) {
            if ((index > 0 && !string_append(out, sv(", "))) ||
                !string_append_cstr(out, fields[index].name)) {
                return false;
            }
        }
        if (!string_append(out, sv(") VALUES ("))) {
            return false;
        }
        for (size_t index = 0; index < count; index++) {
            if ((index > 0 && !string_append(out, sv(", "))) ||
                !string_append(out, sv("?"))) {
                return false;
            }
        }
        if (!string_append(out, sv(")")) || !string_append_char(out, '\"') ||
            !string_append(out, sv("),\n"))) {
            return false;
        }
    }
    if (!generated_append(out, "    .update_sql = sv(\"UPDATE %s SET ",
                          n->plural)) {
        return false;
    }
    for (size_t index = 0; index < count; index++) {
        if ((index > 0 && !string_append(out, sv(", "))) ||
            !generated_append(out, "%s = ?", fields[index].name)) {
            return false;
        }
    }
    if (count == 0 &&
        !string_append(out, sv("updated_at = CURRENT_TIMESTAMP"))) {
        return false;
    }
    if ((count > 0 &&
         !string_append(out,
                        sv(", updated_at = CURRENT_TIMESTAMP WHERE id = ?"))) ||
        (count == 0 && !string_append(out, sv(" WHERE id = ?"))) ||
        !string_append_char(out, '\"') || !string_append(out, sv("),\n"))) {
        return false;
    }
    if (!generated_append(
            out,
            "    .delete_sql = sv(\"DELETE FROM %s WHERE id = ?\"),\n};\n\n",
            n->plural)) {
        return false;
    }
    if (!generated_append(out,
                          "ModelResult %s_find(Context *context, int64_t id, "
                          "%s **post) { return model_find(context, "
                          "&%s_definition, id, (void **)post); }\n",
                          n->singular, n->type_name, n->singular) ||
        !generated_append(
            out,
            "bool %s_all(Context *context, %sArray *posts) { ModelArray result "
            "= {0}; if (posts == NULL || !model_all(context, &%s_definition, "
            "&result)) return false; posts->items = (%s *)result.items; "
            "posts->length = result.length; return true; }\n",
            n->singular, n->type_name, n->singular, n->type_name) ||
        !generated_append(out,
                          "bool %s_insert(Context *context, %s *post) { return "
                          "model_insert(context, &%s_definition, post); }\n",
                          n->singular, n->type_name, n->singular) ||
        !generated_append(
            out,
            "ModelResult %s_update(Context *context, %s *post) { return "
            "model_update(context, &%s_definition, post); }\n",
            n->singular, n->type_name, n->singular) ||
        !generated_append(
            out,
            "ModelResult %s_destroy(Context *context, %s *post) { return "
            "model_destroy(context, &%s_definition, post); }\n",
            n->singular, n->type_name, n->singular)) {
        return false;
    }
    return true;
}

static bool generated_migration(String *out, const Naming *n,
                                const FieldSpec *fields, size_t count) {
    if (!generated_append(
            out, "CREATE TABLE %s (\n    id INTEGER PRIMARY KEY AUTOINCREMENT",
            n->plural)) {
        return false;
    }
    for (size_t index = 0; index < count; index++) {
        if (!generated_append(out, ",\n    %s %s NOT NULL", fields[index].name,
                              naming_sql_type(fields[index].type))) {
            return false;
        }
    }
    return generated_append(
        out, ",\n    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,\n    "
             "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP\n);\n");
}

bool generator_model(const CeasyProject *project, const char *model_name,
                     int field_argc, char **field_argv) {
    Naming naming;
    FieldSpec fields[CEASY_FIELD_CAPACITY];
    size_t field_count;
    char error[256];
    char timestamp[32];
    char migration_relative[PATH_MAX];
    GeneratedFile files[4] = {0};
    bool success;

    if (!naming_model(model_name, &naming, error, sizeof(error)) ||
        !naming_parse_fields(field_argc, field_argv, fields, &field_count,
                             error, sizeof(error))) {
        fprintf(stderr, "error: %s\n", error);
        return false;
    }
    if (!generated_timestamp(timestamp, sizeof(timestamp)) ||
        snprintf(migration_relative, sizeof(migration_relative),
                 "db/migrations/%s_create_%s.sql", timestamp,
                 naming.plural) >= (int)sizeof(migration_relative)) {
        fprintf(stderr, "error: cannot create migration filename\n");
        return false;
    }
    snprintf(files[0].relative, sizeof(files[0].relative), "src/models/%s.h",
             naming.file_stem);
    snprintf(files[1].relative, sizeof(files[1].relative), "src/models/%s.c",
             naming.file_stem);
    snprintf(files[2].relative, sizeof(files[2].relative), "%s",
             migration_relative);
    snprintf(files[3].relative, sizeof(files[3].relative),
             "test/models/%s_test.c", naming.file_stem);
    for (size_t index = 0; index < 4; index++) {
        files[index].content = string_new_heap();
    }
    success =
        generated_model_header(&files[0].content, &naming, fields,
                               field_count) &&
        generated_model_source(&files[1].content, &naming, fields,
                               field_count) &&
        generated_migration(&files[2].content, &naming, fields, field_count) &&
        generated_append(
            &files[3].content,
            "#include \"../../src/models/%s.h\"\n#include <assert.h>\n\nint "
            "main(void) {\n    %s post = {0};\n    assert(post.id == 0);\n    "
            "return 0;\n}\n",
            naming.file_stem, naming.type_name);
    if (success) {
        success = generator_write_files(project, files, 4);
    }
    generated_destroy(files, 4);
    return success;
}

bool generator_migration(const CeasyProject *project, const char *name) {
    char timestamp[32];
    char relative[PATH_MAX];
    GeneratedFile file = {0};
    String content = string_from_heap(sv("-- Write migration SQL here.\n"));

    if (!name || name[0] == '\0') {
        fprintf(stderr, "error: invalid migration name\n");
        return false;
    }
    if (name[strlen(name) - 1] == '_') {
        fprintf(stderr, "error: invalid migration name '%s'\n", name);
        string_destroy(&content);
        return false;
    }
    for (size_t index = 0; name[index] != '\0'; index++) {
        if (!((name[index] >= 'a' && name[index] <= 'z') ||
              (name[index] >= '0' && name[index] <= '9') ||
              name[index] == '_') ||
            (index == 0 && name[index] == '_') ||
            (index > 0 && name[index] == '_' && name[index - 1] == '_')) {
            fprintf(stderr, "error: invalid migration name '%s'\n", name);
            string_destroy(&content);
            return false;
        }
    }
    if (!generated_timestamp(timestamp, sizeof(timestamp)) ||
        snprintf(relative, sizeof(relative), "db/migrations/%s_%s.sql",
                 timestamp, name) >= (int)sizeof(relative)) {
        string_destroy(&content);
        return false;
    }
    snprintf(file.relative, sizeof(file.relative), "%s", relative);
    file.content = content;
    bool success = generator_write_files(project, &file, 1);
    string_destroy(&file.content);
    return success;
}

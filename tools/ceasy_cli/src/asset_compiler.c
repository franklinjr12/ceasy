#define _XOPEN_SOURCE 700

#include "asset_compiler.h"

#include <ceasy/asset/asset.h>
#include <ceasy/string/string.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ASSET_COMPILER_MAX_BYTES (32u * 1024u * 1024u)

typedef struct {
    char *url;
    StringView content_type;
    unsigned char *data;
    size_t length;
} AssetSource;

typedef struct {
    AssetSource *items;
    size_t length;
    size_t capacity;
} AssetSources;

static void asset_sources_destroy(AssetSources *sources) {
    if (sources == NULL) {
        return;
    }
    for (size_t index = 0; index < sources->length; index++) {
        free(sources->items[index].url);
        free(sources->items[index].data);
    }
    free(sources->items);
    memset(sources, 0, sizeof(*sources));
}

static char *asset_duplicate(const char *value) {
    size_t length;
    char *copy;

    if (value == NULL) {
        return NULL;
    }
    length = strlen(value);
    if (length == SIZE_MAX) {
        return NULL;
    }
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, value, length + 1);
    return copy;
}

static bool asset_read_file(const char *path, unsigned char **data,
                            size_t *length) {
    FILE *file = fopen(path, "rb");
    long file_size;
    unsigned char *contents = NULL;

    if (data != NULL) {
        *data = NULL;
    }
    if (length != NULL) {
        *length = 0;
    }
    if (file == NULL) {
        fprintf(stderr, "error: could not read %s: %s\n", path,
                strerror(errno));
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (file_size = ftell(file)) < 0 ||
        (unsigned long)file_size > ASSET_COMPILER_MAX_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "error: asset too large or unreadable: %s\n", path);
        fclose(file);
        return false;
    }
    if (file_size > 0) {
        contents = malloc((size_t)file_size);
        if (contents == NULL) {
            fprintf(stderr, "error: could not allocate asset: %s\n", path);
            fclose(file);
            return false;
        }
        if (fread(contents, 1, (size_t)file_size, file) != (size_t)file_size) {
            fprintf(stderr, "error: could not read %s: %s\n", path,
                    ferror(file) ? strerror(errno) : "short read");
            free(contents);
            fclose(file);
            return false;
        }
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "error: could not close %s: %s\n", path,
                strerror(errno));
        free(contents);
        return false;
    }
    *data = contents;
    *length = (size_t)file_size;
    return true;
}

static bool asset_sources_push(AssetSources *sources, AssetSource source) {
    AssetSource *replacement;
    size_t new_capacity;

    if (sources->length == sources->capacity) {
        new_capacity = sources->capacity == 0 ? 16 : sources->capacity * 2;
        if (new_capacity < sources->capacity ||
            new_capacity > SIZE_MAX / sizeof(*replacement)) {
            return false;
        }
        replacement =
            realloc(sources->items, new_capacity * sizeof(*replacement));
        if (replacement == NULL) {
            return false;
        }
        sources->items = replacement;
        sources->capacity = new_capacity;
    }
    sources->items[sources->length++] = source;
    return true;
}

static bool asset_scan(AssetSources *sources, const char *root,
                       const char *relative) {
    char directory[PATH_MAX];
    DIR *entries;
    struct dirent *entry;
    bool success = true;

    if (snprintf(directory, sizeof(directory), "%s/public%s%s", root,
                 relative[0] == '\0' ? "" : "/",
                 relative) >= (int)sizeof(directory)) {
        fprintf(stderr, "error: public asset path is too long\n");
        return false;
    }
    entries = opendir(directory);
    if (entries == NULL) {
        if (relative[0] == '\0' && errno == ENOENT) {
            return true;
        }
        fprintf(stderr, "error: could not scan %s: %s\n", directory,
                strerror(errno));
        return false;
    }
    while (success && (entry = readdir(entries)) != NULL) {
        char child[PATH_MAX];
        char full[PATH_MAX];
        char url[PATH_MAX + 2];
        struct stat status;
        AssetSource source = {0};

        full[0] = '\0';

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (snprintf(child, sizeof(child), "%s%s%s", relative,
                     relative[0] == '\0' ? "" : "/",
                     entry->d_name) >= (int)sizeof(child) ||
            snprintf(full, sizeof(full), "%s/public/%s", root, child) >=
                (int)sizeof(full) ||
            lstat(full, &status) != 0) {
            fprintf(stderr, "error: could not inspect %s\n", full);
            success = false;
            break;
        }
        if (S_ISDIR(status.st_mode)) {
            success = asset_scan(sources, root, child);
            continue;
        }
        if (!S_ISREG(status.st_mode) ||
            snprintf(url, sizeof(url), "/%s", child) >= (int)sizeof(url)) {
            continue;
        }
        source.url = asset_duplicate(url);
        source.content_type = asset_content_type(stringv_from_cstr(url));
        if (source.url == NULL ||
            !asset_read_file(full, &source.data, &source.length) ||
            !asset_sources_push(sources, source)) {
            free(source.url);
            free(source.data);
            fprintf(stderr, "error: could not compile asset %s\n", full);
            success = false;
            break;
        }
    }
    closedir(entries);
    return success;
}

static int asset_source_compare(const void *left, const void *right) {
    const AssetSource *a = left;
    const AssetSource *b = right;

    return strcmp(a->url, b->url);
}

static bool asset_append_byte_array(String *output, const char *name,
                                    const unsigned char *data, size_t length) {
    if (!string_append_format(output, "static const unsigned char %s[] = {\n",
                              name)) {
        return false;
    }
    if (length == 0 && !string_append(output, sv("    0x00,\n"))) {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        if (!string_append_format(output, "    0x%02x,%s",
                                  (unsigned int)data[index],
                                  index % 12 == 11 ? "\n" : "")) {
            return false;
        }
    }
    return string_append(output, sv("};\n\n"));
}

static bool asset_append_view(String *output, const char *field,
                              const char *name, size_t length) {
    return string_append_format(
        output, ".%s = {.data = (const char *)%s, .length = %zu}, ", field,
        name, length);
}

static bool asset_generate(String *output, const AssetSources *sources) {
    if (!string_append(output, sv("/*\n * Generated by Ceasy.\n *\n * Source:\n"
                                  " *   public/**\n *\n"
                                  " * Do not edit this file manually.\n */\n\n"
                                  "#include <ceasy/asset/asset.h>\n\n"))) {
        return false;
    }
    for (size_t index = 0; index < sources->length; index++) {
        char path_name[64];
        char type_name[64];
        char data_name[64];

        if (snprintf(path_name, sizeof(path_name), "ceasy_asset_path_%zu",
                     index) >= (int)sizeof(path_name) ||
            snprintf(type_name, sizeof(type_name), "ceasy_asset_type_%zu",
                     index) >= (int)sizeof(type_name) ||
            snprintf(data_name, sizeof(data_name), "ceasy_asset_data_%zu",
                     index) >= (int)sizeof(data_name) ||
            !asset_append_byte_array(
                output, path_name,
                (const unsigned char *)sources->items[index].url,
                strlen(sources->items[index].url)) ||
            !asset_append_byte_array(
                output, type_name,
                (const unsigned char *)sources->items[index].content_type.data,
                sources->items[index].content_type.length) ||
            !asset_append_byte_array(output, data_name,
                                     sources->items[index].data,
                                     sources->items[index].length)) {
            return false;
        }
    }
    if (sources->length == 0 &&
        !string_append(output,
                       sv("static const EmbeddedAsset ceasy_assets[1] = "
                          "{{0}};\n\n"))) {
        return false;
    }
    if (sources->length > 0 &&
        !string_append(output, sv("static const EmbeddedAsset ceasy_assets[] = "
                                  "{\n"))) {
        return false;
    }
    for (size_t index = 0; index < sources->length; index++) {
        char path_name[64];
        char type_name[64];
        char data_name[64];

        snprintf(path_name, sizeof(path_name), "ceasy_asset_path_%zu", index);
        snprintf(type_name, sizeof(type_name), "ceasy_asset_type_%zu", index);
        snprintf(data_name, sizeof(data_name), "ceasy_asset_data_%zu", index);
        if (!string_append(output, sv("    {")) ||
            !asset_append_view(output, "path", path_name,
                               strlen(sources->items[index].url)) ||
            !asset_append_view(output, "content_type", type_name,
                               sources->items[index].content_type.length) ||
            !string_append_format(output, ".data = %s, .length = %zu},\n",
                                  data_name, sources->items[index].length)) {
            return false;
        }
    }
    if (sources->length > 0 && !string_append(output, sv("};\n\n"))) {
        return false;
    }
    return string_append_format(
        output,
        "static const AssetBundle ceasy_bundle = "
        "{.assets = ceasy_assets, .asset_count = %zu};\n\n"
        "const AssetBundle *ceasy_asset_bundle(void) {\n"
        "    return &ceasy_bundle;\n}\n",
        sources->length);
}

static bool asset_write(const CeasyProject *project, const String *output) {
    char directory[PATH_MAX];
    char target[PATH_MAX];
    char temporary[PATH_MAX];
    FILE *file;
    bool success;

    if (!project_path(project, "src/generated/ceasy_assets.c", target,
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
        fprintf(stderr, "error: cannot write %s: %s\n", temporary,
                strerror(errno));
        return false;
    }
    success = fwrite(output->data, 1, output->length, file) == output->length &&
              fflush(file) == 0;
    if (fclose(file) != 0) {
        success = false;
    }
    if (!success) {
        unlink(temporary);
        fprintf(stderr, "error: could not finish writing %s\n", target);
        return false;
    }
    if (rename(temporary, target) != 0) {
        unlink(temporary);
        fprintf(stderr, "error: could not replace %s: %s\n", target,
                strerror(errno));
        return false;
    }
    printf("compile src/generated/ceasy_assets.c\n");
    return true;
}

bool asset_compiler_run(const CeasyProject *project) {
    AssetSources sources = {0};
    String output = string_new_heap();
    bool success = project != NULL && asset_scan(&sources, project->root, "");

    if (success) {
        qsort(sources.items, sources.length, sizeof(sources.items[0]),
              asset_source_compare);
        for (size_t index = 1; index < sources.length; index++) {
            if (strcmp(sources.items[index - 1].url,
                       sources.items[index].url) == 0) {
                fprintf(stderr, "error: duplicate asset path %s\n",
                        sources.items[index].url);
                success = false;
                break;
            }
        }
    }
    success = success && asset_generate(&output, &sources) &&
              asset_write(project, &output);
    string_destroy(&output);
    asset_sources_destroy(&sources);
    return success;
}

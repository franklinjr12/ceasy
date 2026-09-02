#define _XOPEN_SOURCE 700

#include "ceasy/asset/asset.h"

#include <ceasy/ceasy.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CEASY_MAX_ASSET_BYTES (32u * 1024u * 1024u)

static bool asset_hex_digit(char value, unsigned char *result) {
    if (value >= '0' && value <= '9') {
        *result = (unsigned char)(value - '0');
        return true;
    }
    if (value >= 'a' && value <= 'f') {
        *result = (unsigned char)(value - 'a' + 10);
        return true;
    }
    if (value >= 'A' && value <= 'F') {
        *result = (unsigned char)(value - 'A' + 10);
        return true;
    }
    return false;
}

static bool asset_normalize_path(Context *context, StringView request_path,
                                 StringView *normalized) {
    char *data;
    size_t output_length = 0;
    size_t segment_start;

    if (normalized != NULL) {
        *normalized = (StringView){0};
    }
    if (context == NULL || context->arena == NULL || normalized == NULL ||
        request_path.data == NULL || request_path.length < 2 ||
        request_path.data[0] != '/' || request_path.length > SIZE_MAX - 1) {
        return false;
    }
    data = arena_alloc(context->arena, request_path.length + 1);
    if (data == NULL) {
        return false;
    }
    for (size_t index = 0; index < request_path.length; index++) {
        unsigned char high;
        unsigned char low;
        char value = request_path.data[index];

        if (value == '%') {
            if (index + 2 >= request_path.length ||
                !asset_hex_digit(request_path.data[index + 1], &high) ||
                !asset_hex_digit(request_path.data[index + 2], &low)) {
                return false;
            }
            value = (char)((high << 4) | low);
            index += 2;
        }
        if (value == '\0' || value == '\\' || value == ':') {
            return false;
        }
        data[output_length++] = value;
    }
    data[output_length] = '\0';

    segment_start = 1;
    for (size_t index = 1; index <= output_length; index++) {
        if (index != output_length && data[index] != '/') {
            continue;
        }
        if (index == segment_start ||
            (index - segment_start == 1 && data[segment_start] == '.') ||
            (index - segment_start == 2 && data[segment_start] == '.' &&
             data[segment_start + 1] == '.')) {
            return false;
        }
        segment_start = index + 1;
    }
    *normalized = (StringView){.data = data, .length = output_length};
    return true;
}

static bool asset_path_within_root(const char *root, const char *path) {
    size_t root_length = strlen(root);

    return strncmp(root, path, root_length) == 0 &&
           (path[root_length] == '\0' || path[root_length] == '/');
}

static AssetLoadResult asset_load_filesystem_normalized(Context *context,
                                                        StringView path,
                                                        AssetData *asset) {
    char public_directory[PATH_MAX];
    char public_realpath[PATH_MAX];
    char filename[PATH_MAX];
    char filename_realpath[PATH_MAX];
    char cwd[PATH_MAX];
    FILE *input;
    long file_size;
    unsigned char *data = NULL;

    if (asset != NULL) {
        *asset = (AssetData){0};
    }
    if (context == NULL || context->arena == NULL || asset == NULL ||
        path.data == NULL || path.length < 2 ||
        getcwd(cwd, sizeof(cwd)) == NULL ||
        snprintf(public_directory, sizeof(public_directory), "%s/public",
                 cwd) >= (int)sizeof(public_directory)) {
        return ASSET_LOAD_ERROR;
    }
    if (realpath(public_directory, public_realpath) == NULL) {
        return errno == ENOENT || errno == ENOTDIR ? ASSET_LOAD_MISSING
                                                   : ASSET_LOAD_ERROR;
    }
    if (snprintf(filename, sizeof(filename), "%s%.*s", public_realpath,
                 (int)path.length, path.data) >= (int)sizeof(filename)) {
        return ASSET_LOAD_ERROR;
    }
    if (realpath(filename, filename_realpath) == NULL) {
        return errno == ENOENT || errno == ENOTDIR ? ASSET_LOAD_MISSING
                                                   : ASSET_LOAD_ERROR;
    }
    if (!asset_path_within_root(public_realpath, filename_realpath)) {
        return ASSET_LOAD_ERROR;
    }
    input = fopen(filename_realpath, "rb");
    if (input == NULL) {
        return errno == ENOENT || errno == ENOTDIR ? ASSET_LOAD_MISSING
                                                   : ASSET_LOAD_ERROR;
    }
    if (fseek(input, 0, SEEK_END) != 0 || (file_size = ftell(input)) < 0 ||
        (unsigned long)file_size > CEASY_MAX_ASSET_BYTES ||
        fseek(input, 0, SEEK_SET) != 0) {
        fclose(input);
        return ASSET_LOAD_ERROR;
    }
    if (file_size > 0) {
        data = (unsigned char *)arena_alloc(context->arena, (size_t)file_size);
        if (data == NULL ||
            fread(data, 1, (size_t)file_size, input) != (size_t)file_size) {
            fclose(input);
            return ASSET_LOAD_ERROR;
        }
    }
    if (fclose(input) != 0) {
        return ASSET_LOAD_ERROR;
    }
    asset->content_type = asset_content_type(path);
    asset->data = data;
    asset->length = (size_t)file_size;
    return ASSET_LOAD_FOUND;
}

AssetLoadResult asset_load_filesystem(Context *context, StringView request_path,
                                      AssetData *asset) {
    StringView normalized;

    if (!asset_normalize_path(context, request_path, &normalized)) {
        if (asset != NULL) {
            *asset = (AssetData){0};
        }
        return ASSET_LOAD_ERROR;
    }
    return asset_load_filesystem_normalized(context, normalized, asset);
}

const EmbeddedAsset *asset_bundle_find(const AssetBundle *bundle,
                                       StringView path) {
    if (bundle == NULL || (bundle->asset_count > 0 && bundle->assets == NULL)) {
        return NULL;
    }
    for (size_t index = 0; index < bundle->asset_count; index++) {
        if (stringv_equal(bundle->assets[index].path, path)) {
            return &bundle->assets[index];
        }
    }
    return NULL;
}

bool asset_serve(Context *context, StringView request_path) {
    StringView normalized;
    AssetData filesystem;
    AssetLoadResult result;
    const EmbeddedAsset *embedded;
    const AssetBundle *bundle;

    if (context == NULL ||
        !asset_normalize_path(context, request_path, &normalized)) {
        return false;
    }
    result = asset_load_filesystem_normalized(context, normalized, &filesystem);
    if (result == ASSET_LOAD_FOUND) {
        return context_send_bytes(context, sv("200 OK"),
                                  filesystem.content_type, filesystem.data,
                                  filesystem.length);
    }
    if (result == ASSET_LOAD_ERROR) {
        return false;
    }
    bundle = ceasy_asset_bundle();
    embedded = asset_bundle_find(bundle, normalized);
    if (embedded == NULL) {
        return false;
    }
    return context_send_bytes(context, sv("200 OK"), embedded->content_type,
                              embedded->data, embedded->length);
}

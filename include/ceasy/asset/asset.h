#ifndef CEASY_ASSET_ASSET_H
#define CEASY_ASSET_ASSET_H

#include <stdbool.h>
#include <stddef.h>

#include <ceasy/string/string.h>

typedef struct Context Context;

typedef struct {
    StringView content_type;
    const unsigned char *data;
    size_t length;
} AssetData;

typedef enum {
    ASSET_LOAD_MISSING = 0,
    ASSET_LOAD_FOUND,
    ASSET_LOAD_ERROR
} AssetLoadResult;

typedef struct {
    StringView path;
    StringView content_type;
    const unsigned char *data;
    size_t length;
} EmbeddedAsset;

typedef struct {
    const EmbeddedAsset *assets;
    size_t asset_count;
} AssetBundle;

StringView asset_content_type(StringView path);
const EmbeddedAsset *asset_bundle_find(const AssetBundle *bundle,
                                       StringView path);
const AssetBundle *ceasy_asset_bundle(void);

AssetLoadResult asset_load_filesystem(Context *context, StringView request_path,
                                      AssetData *asset);
bool asset_serve(Context *context, StringView request_path);

#endif

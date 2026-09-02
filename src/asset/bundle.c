#include "ceasy/asset/asset.h"

static const AssetBundle ceasy_empty_bundle = {.assets = NULL,
                                               .asset_count = 0};

__attribute__((weak)) const AssetBundle *ceasy_asset_bundle(void) {
    return &ceasy_empty_bundle;
}

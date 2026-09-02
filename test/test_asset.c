#define _XOPEN_SOURCE 700

#include "ceasy/ceasy.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern char *mkdtemp(char *template);

static void write_bytes(const char *path, const unsigned char *data,
                        size_t length) {
    FILE *file = fopen(path, "wb");

    assert(file != NULL);
    assert(fwrite(data, 1, length, file) == length);
    assert(fclose(file) == 0);
}

static void test_mime_types(void) {
    assert(stringv_equal(asset_content_type(sv("/app.css")),
                         sv("text/css; charset=utf-8")));
    assert(stringv_equal(asset_content_type(sv("/app.JS")),
                         sv("text/javascript; charset=utf-8")));
    assert(stringv_equal(asset_content_type(sv("/page.html")),
                         sv("text/html; charset=utf-8")));
    assert(stringv_equal(asset_content_type(sv("/data.json")),
                         sv("application/json; charset=utf-8")));
    assert(stringv_equal(asset_content_type(sv("/icon.svg")),
                         sv("image/svg+xml")));
    assert(
        stringv_equal(asset_content_type(sv("/image.png")), sv("image/png")));
    assert(
        stringv_equal(asset_content_type(sv("/image.jpg")), sv("image/jpeg")));
    assert(
        stringv_equal(asset_content_type(sv("/font.woff2")), sv("font/woff2")));
    assert(stringv_equal(asset_content_type(sv("/unknown.bin")),
                         sv("application/octet-stream")));
}

int main(void) {
    char original[PATH_MAX];
    char root_template[] = "/tmp/ceasy-asset-XXXXXX";
    char *root = mkdtemp(root_template);
    const unsigned char first[] = {0x00, 0xff, 0x00, 0x7f};
    const unsigned char second[] = {0x42};
    Context context = {0};
    Arena arena;
    AssetData asset;
    AssetBundle bundle;
    EmbeddedAsset embedded;

    assert(root != NULL);
    assert(getcwd(original, sizeof(original)) != NULL);
    assert(chdir(root) == 0);
    assert(mkdir("public", 0755) == 0);
    assert(mkdir("public/images", 0755) == 0);
    write_bytes("public/images/test.bin", first, sizeof(first));
    assert(arena_init(&arena, 4096));
    context.arena = &arena;

    test_mime_types();
    assert(asset_load_filesystem(&context, sv("/images/test.bin"), &asset) ==
           ASSET_LOAD_FOUND);
    assert(asset.length == sizeof(first));
    assert(memcmp(asset.data, first, sizeof(first)) == 0);
    write_bytes("public/images/test.bin", second, sizeof(second));
    assert(asset_load_filesystem(&context, sv("/images/test.bin"), &asset) ==
           ASSET_LOAD_FOUND);
    assert(asset.length == sizeof(second));
    assert(asset.data[0] == second[0]);

    assert(asset_load_filesystem(&context, sv("/../secret"), &asset) ==
           ASSET_LOAD_ERROR);
    assert(asset_load_filesystem(&context, sv("/%2e%2e/secret"), &asset) ==
           ASSET_LOAD_ERROR);
    assert(asset_load_filesystem(&context, sv("/%2E%2E/secret"), &asset) ==
           ASSET_LOAD_ERROR);
    assert(asset_load_filesystem(&context, sv("/images/../../secret"),
                                 &asset) == ASSET_LOAD_ERROR);
    assert(asset_load_filesystem(&context, sv("/images/%5ctest.bin"), &asset) ==
           ASSET_LOAD_ERROR);

    embedded = (EmbeddedAsset){.path = sv("/embedded.bin"),
                               .content_type = sv("application/octet-stream"),
                               .data = first,
                               .length = sizeof(first)};
    bundle = (AssetBundle){.assets = &embedded, .asset_count = 1};
    assert(asset_bundle_find(&bundle, sv("/embedded.bin")) == &embedded);
    assert(asset_bundle_find(&bundle, sv("/missing.bin")) == NULL);

    arena_destroy(&arena);
    assert(chdir(original) == 0);
    {
        char path[PATH_MAX];

        assert(snprintf(path, sizeof(path), "%s/public/images/test.bin", root) >
               0);
        assert(unlink(path) == 0);
        assert(snprintf(path, sizeof(path), "%s/public/images", root) > 0);
        assert(rmdir(path) == 0);
        assert(snprintf(path, sizeof(path), "%s/public", root) > 0);
        assert(rmdir(path) == 0);
    }
    assert(rmdir(root) == 0);
    return 0;
}

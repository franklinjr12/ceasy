#define _XOPEN_SOURCE 700

#include "../src/asset_compiler.h"

#include <ceasy/ceasy.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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

static String read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    long length;
    String result = string_new_heap();

    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length >= 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    assert(string_reserve(&result, (size_t)length));
    if (length > 0) {
        assert(fread(result.data, 1, (size_t)length, file) == (size_t)length);
    }
    result.length = (size_t)length;
    result.data[result.length] = '\0';
    assert(fclose(file) == 0);
    return result;
}

int main(void) {
    char original[PATH_MAX];
    char root_template[] = "/tmp/ceasy-asset-compiler-XXXXXX";
    char *root = mkdtemp(root_template);
    CeasyProject project = {0};
    const unsigned char binary[] = {0x00, 0xff, 0x7f};
    String first;
    String second;
    char command[PATH_MAX * 2];
    char generated[PATH_MAX];

    assert(root != NULL);
    assert(getcwd(original, sizeof(original)) != NULL);
    strcpy(project.root, root);
    assert(chdir(root) == 0);
    assert(mkdir("public", 0755) == 0);
    assert(mkdir("public/images", 0755) == 0);
    assert(mkdir("src", 0755) == 0);
    write_bytes("public/styles.css", (const unsigned char *)"A", 1);
    write_bytes("public/images/test.bin", binary, sizeof(binary));
    assert(asset_compiler_run(&project));
    assert(snprintf(generated, sizeof(generated),
                    "%s/src/generated/ceasy_assets.c", root) > 0);
    first = read_file(generated);
    assert(stringv_contains(string_as_view(&first), sv("0x2f,\n")));
    assert(stringv_contains(string_as_view(&first), sv("0xff")));
    assert(snprintf(command, sizeof(command),
                    "clang -std=c17 -I/workspace/include -c %s -o %s/bundle.o",
                    generated, root) > 0);
    assert(system(command) == 0);
    assert(asset_compiler_run(&project));
    second = read_file(generated);
    assert(stringv_equal(string_as_view(&first), string_as_view(&second)));

    string_destroy(&first);
    string_destroy(&second);
    assert(unlink("src/generated/ceasy_assets.c") == 0);
    assert(rmdir("src/generated") == 0);
    assert(unlink("public/images/test.bin") == 0);
    assert(unlink("public/styles.css") == 0);
    assert(rmdir("public/images") == 0);
    assert(rmdir("public") == 0);
    assert(unlink("bundle.o") == 0);
    assert(rmdir("src") == 0);
    assert(chdir(original) == 0);
    assert(rmdir(root) == 0);
    return 0;
}

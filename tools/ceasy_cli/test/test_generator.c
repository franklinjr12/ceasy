#include "generator.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool exists(const char *path) { return access(path, F_OK) == 0; }

static void remove_directory_files(const char *directory) {
    DIR *entries = opendir(directory);
    struct dirent *entry;
    char path[PATH_MAX];

    assert(entries != NULL);
    while ((entry = readdir(entries)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        assert(snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) >
               0);
        assert(unlink(path) == 0);
    }
    assert(closedir(entries) == 0);
    assert(rmdir(directory) == 0);
}

int main(void) {
    char root_template[] = "/tmp/ceasy-generator-test-XXXXXX";
    char *root = mkdtemp(root_template);
    char path[PATH_MAX];
    char *fields[] = {"published:bool", "views:integer"};
    CeasyProject project = {0};

    assert(root != NULL);
    assert(strlen(root) < sizeof(project.root));
    strcpy(project.root, root);
    assert(generator_model(&project, "User", 2, fields));
    assert(snprintf(path, sizeof(path), "%s/src/models/user.h", root) > 0);
    assert(exists(path));
    assert(snprintf(path, sizeof(path), "%s/src/models/user.c", root) > 0);
    assert(exists(path));
    assert(snprintf(path, sizeof(path), "%s/test/models/user_test.c", root) >
           0);
    assert(exists(path));
    assert(!generator_model(&project, "User", 2, fields));
    assert(generator_migration(&project, "add_flags"));

    assert(snprintf(path, sizeof(path), "%s/db/migrations", root) > 0);
    remove_directory_files(path);
    assert(snprintf(path, sizeof(path), "%s/src/models", root) > 0);
    remove_directory_files(path);
    assert(snprintf(path, sizeof(path), "%s/test/models", root) > 0);
    remove_directory_files(path);
    assert(snprintf(path, sizeof(path), "%s/db", root) > 0);
    assert(rmdir(path) == 0);
    assert(snprintf(path, sizeof(path), "%s/src", root) > 0);
    assert(rmdir(path) == 0);
    assert(snprintf(path, sizeof(path), "%s/test", root) > 0);
    assert(rmdir(path) == 0);
    assert(rmdir(root) == 0);
    return 0;
}

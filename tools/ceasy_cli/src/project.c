#include "project.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool project_has_cdev(const char *directory) {
    char path[PATH_MAX];
    int written;

    written = snprintf(path, sizeof(path), "%s/cdev.conf", directory);
    return written >= 0 && (size_t)written < sizeof(path) &&
           access(path, F_OK) == 0;
}

static bool project_parent(char *directory) {
    char *separator = strrchr(directory, '/');

    if (separator == NULL) {
        return false;
    }
    if (separator == directory) {
        directory[1] = '\0';
        return false;
    }
    *separator = '\0';
    return true;
}

bool project_find_root(CeasyProject *project) {
    char directory[PATH_MAX];

    if (project == NULL || getcwd(directory, sizeof(directory)) == NULL) {
        return false;
    }
    while (true) {
        if (project_has_cdev(directory)) {
            size_t length = strlen(directory);

            if (length >= sizeof(project->root)) {
                return false;
            }
            memcpy(project->root, directory, length + 1);
            return true;
        }
        if (!project_parent(directory)) {
            return false;
        }
    }
}

bool project_path(const CeasyProject *project, const char *relative, char *path,
                  size_t path_size) {
    int written;

    if (project == NULL || relative == NULL || path == NULL || path_size == 0 ||
        relative[0] == '/') {
        return false;
    }
    written = snprintf(path, path_size, "%s/%s", project->root, relative);
    return written >= 0 && (size_t)written < path_size;
}

#ifndef CEASY_CLI_PROJECT_H
#define CEASY_CLI_PROJECT_H

#include <stdbool.h>
#include <stddef.h>

#include <limits.h>

typedef struct {
    char root[PATH_MAX];
} CeasyProject;

bool project_find_root(CeasyProject *project);
bool project_path(const CeasyProject *project, const char *relative, char *path,
                  size_t path_size);

#endif

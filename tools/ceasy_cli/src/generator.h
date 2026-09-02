#ifndef CEASY_CLI_GENERATOR_H
#define CEASY_CLI_GENERATOR_H

#include <stdbool.h>

#include "project.h"

bool generator_model(const CeasyProject *project, const char *model_name,
                     int field_argc, char **field_argv);
bool generator_migration(const CeasyProject *project, const char *name);

#endif

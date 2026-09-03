#ifndef CEASY_CLI_SESSIONS_H
#define CEASY_CLI_SESSIONS_H

#include <stdbool.h>

#include "project.h"

bool sessions_install(const CeasyProject *project);
bool sessions_cleanup(const CeasyProject *project);

#endif

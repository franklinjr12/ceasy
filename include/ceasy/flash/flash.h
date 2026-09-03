#ifndef CEASY_FLASH_H
#define CEASY_FLASH_H

#include <ceasy/string/string.h>

typedef struct Context Context;

bool flash_set(Context *context, StringView key, StringView message);
StringView flash_get(Context *context, StringView key);

#endif

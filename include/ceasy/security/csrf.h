#ifndef CEASY_SECURITY_CSRF_H
#define CEASY_SECURITY_CSRF_H

#include <ceasy/string/string.h>

typedef struct Context Context;

StringView csrf_token(Context *context);
bool csrf_verify(Context *context, StringView token);

#endif

#ifndef CEASY_AUTH_H
#define CEASY_AUTH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Context Context;

bool auth_login(Context *context, int64_t user_id);
bool auth_logout(Context *context);
bool auth_signed_in(Context *context);
bool auth_user_id(Context *context, int64_t *user_id);

#endif

#ifndef CEASY_SESSION_H
#define CEASY_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include <ceasy/collection/string_map.h>
#include <ceasy/string/string.h>

typedef struct Context Context;

typedef struct {
    StringMap values;
    String token;
    bool loaded;
    bool exists;
    bool dirty;
    bool regenerate;
    bool destroy;
    int64_t expires_at;
} Session;

bool session_set_string(Context *context, StringView key, StringView value);
bool session_get_string(Context *context, StringView key, StringView *value);
bool session_set_int64(Context *context, StringView key, int64_t value);
bool session_get_int64(Context *context, StringView key, int64_t *value);
bool session_set_bool(Context *context, StringView key, bool value);
bool session_get_bool(Context *context, StringView key, bool *value);
bool session_delete(Context *context, StringView key);
bool session_clear(Context *context);
bool session_regenerate(Context *context);
bool session_destroy(Context *context);
bool session_commit(Context *context);

#endif

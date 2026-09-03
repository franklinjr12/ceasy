#include <ceasy/session/session.h>

#include <ceasy/config/config.h>
#include <ceasy/context.h>

#include <sodium.h>

#include <limits.h>
#include <string.h>
#include <time.h>

#define SESSION_MAX_BYTES (16u * 1024u)
#define SESSION_TOKEN_BYTES 32
#define SESSION_KEY_PREFIX "__ceasy."

static bool session_map_init(Context *context) {
    return context->session.values.allocator.alloc != NULL ||
           sm_init_in(&context->session.values, context->arena);
}

static bool session_digest(StringView token, unsigned char digest[32]) {
    return token.data != NULL && token.length > 0 && sodium_init() >= 0 &&
           crypto_generichash(digest, 32, (const unsigned char *)token.data,
                              token.length, NULL, 0) == 0;
}

static bool session_generate_token(Context *context) {
    unsigned char bytes[SESSION_TOKEN_BYTES];
    char encoded[64];
    size_t encoded_length;

    if (context == NULL || context->arena == NULL || sodium_init() < 0) {
        return false;
    }
    randombytes_buf(bytes, sizeof(bytes));
    sodium_bin2base64(encoded, sizeof(encoded), bytes, sizeof(bytes),
                      sodium_base64_VARIANT_URLSAFE_NO_PADDING);
    encoded_length = strlen(encoded);
    context->session.token =
        string_from_in(context->arena,
                       (StringView){.data = encoded, .length = encoded_length});
    return context->session.token.data != NULL;
}

static bool session_delete_digest(Context *context, StringView token) {
    unsigned char digest[32];
    DatabaseStatement statement = {0};
    bool success;

    if (!session_digest(token, digest) || context->database == NULL ||
        !database_prepare(
            context->database, &statement,
            sv("DELETE FROM ceasy_sessions WHERE token_digest = ?")) ||
        !database_bind_blob(&statement, 1, digest, sizeof(digest))) {
        database_statement_destroy(&statement);
        return false;
    }
    success = database_execute(&statement);
    database_statement_destroy(&statement);
    return success;
}

static bool session_read_u16(const unsigned char *data, size_t length,
                             size_t *offset, uint16_t *value) {
    if (*offset > length || length - *offset < 2) {
        return false;
    }
    *value = (uint16_t)((data[*offset] << 8) | data[*offset + 1]);
    *offset += 2;
    return true;
}
static bool session_read_u32(const unsigned char *data, size_t length,
                             size_t *offset, uint32_t *value) {
    if (*offset > length || length - *offset < 4) {
        return false;
    }
    *value = ((uint32_t)data[*offset] << 24) |
             ((uint32_t)data[*offset + 1] << 16) |
             ((uint32_t)data[*offset + 2] << 8) | data[*offset + 3];
    *offset += 4;
    return true;
}
static bool session_read_u64(const unsigned char *data, size_t length,
                             size_t *offset, uint64_t *value) {
    *value = 0;
    if (*offset > length || length - *offset < 8) {
        return false;
    }
    for (size_t index = 0; index < 8; index++) {
        *value = (*value << 8) | data[*offset + index];
    }
    *offset += 8;
    return true;
}

static bool session_deserialize(Context *context, const void *blob,
                                size_t length) {
    const unsigned char *data = blob;
    size_t offset = 0;
    uint16_t count;

    if (data == NULL || length == 0 || length > SESSION_MAX_BYTES ||
        data[0] != 1 || !session_read_u16(data, length, &(size_t){1}, &count) ||
        !session_map_init(context)) {
        return false;
    }
    offset = 3;
    for (uint16_t index = 0; index < count; index++) {
        uint16_t key_length;
        StringView key;
        unsigned char type;
        uint32_t value_length;
        uint64_t integer;

        if (!session_read_u16(data, length, &offset, &key_length) ||
            offset > length || length - offset < key_length + 1) {
            return false;
        }
        key = (StringView){.data = (const char *)data + offset,
                           .length = key_length};
        offset += key_length;
        type = data[offset++];
        if (type == 1) {
            if (!session_read_u32(data, length, &offset, &value_length) ||
                value_length > length - offset ||
                !sm_set_string_view(
                    &context->session.values, key,
                    (StringView){.data = (const char *)data + offset,
                                 .length = value_length})) {
                return false;
            }
            offset += value_length;
        } else if (type == 2) {
            if (!session_read_u64(data, length, &offset, &integer) ||
                !sm_set_value(&context->session.values, key,
                              sm_int((int64_t)integer))) {
                return false;
            }
        } else if (type == 3) {
            if (offset >= length || data[offset] > 1 ||
                !sm_set_value(&context->session.values, key,
                              sm_bool(data[offset++] != 0))) {
                return false;
            }
        } else {
            return false;
        }
    }
    return offset == length;
}

static bool session_write_u16(String *output, size_t value) {
    return value <= UINT16_MAX &&
           string_append_char(output, (char)(value >> 8)) &&
           string_append_char(output, (char)value);
}
static bool session_write_u32(String *output, size_t value) {
    return value <= UINT32_MAX &&
           string_append_char(output, (char)(value >> 24)) &&
           string_append_char(output, (char)(value >> 16)) &&
           string_append_char(output, (char)(value >> 8)) &&
           string_append_char(output, (char)value);
}
static bool session_write_u64(String *output, int64_t value) {
    uint64_t integer = (uint64_t)value;
    for (int shift = 56; shift >= 0; shift -= 8) {
        if (!string_append_char(output, (char)(integer >> shift))) {
            return false;
        }
    }
    return true;
}

static bool session_serialize(Context *context, String *output) {
    StringMapIterator iterator = sm_iterator();
    StringView key;
    StringMapValue *value;
    size_t count = sm_length(&context->session.values);

    *output = string_new_in(context->arena);
    if (count > UINT16_MAX || !string_append_char(output, 1) ||
        !session_write_u16(output, count)) {
        return false;
    }
    while (sm_next(&context->session.values, &iterator, &key, &value)) {
        if (key.length > UINT16_MAX || !session_write_u16(output, key.length) ||
            !string_append(output, key)) {
            return false;
        }
        if (value->type == SM_VALUE_STRING) {
            if (value->string.length > UINT32_MAX ||
                !string_append_char(output, 1) ||
                !session_write_u32(output, value->string.length) ||
                !string_append(output, value->string)) {
                return false;
            }
        } else if (value->type == SM_VALUE_INT64) {
            if (!string_append_char(output, 2) ||
                !session_write_u64(output, value->integer)) {
                return false;
            }
        } else if (value->type == SM_VALUE_BOOL) {
            if (!string_append_char(output, 3) ||
                !string_append_char(output, value->boolean ? 1 : 0)) {
                return false;
            }
        } else {
            return false;
        }
        if (output->length > SESSION_MAX_BYTES) {
            return false;
        }
    }
    return true;
}

static bool session_load(Context *context) {
    unsigned char digest[32];
    DatabaseStatement statement = {0};
    StringView cookie;
    DatabaseStepResult step;
    int64_t expires;
    size_t blob_length;
    const void *blob;

    if (context == NULL || context->arena == NULL ||
        context->database == NULL) {
        return false;
    }
    context->session.loaded = true;
    if (!session_map_init(context)) {
        return false;
    }
    cookie = context_cookie(context, ceasy_session_cookie_name());
    if (cookie.length == 0 || !session_digest(cookie, digest)) {
        return true;
    }
    if (!database_prepare(context->database, &statement,
                          sv("SELECT data, expires_at FROM ceasy_sessions "
                             "WHERE token_digest = ?")) ||
        !database_bind_blob(&statement, 1, digest, sizeof(digest))) {
        database_statement_destroy(&statement);
        return false;
    }
    step = database_step(&statement);
    if (step == DATABASE_STEP_DONE) {
        database_statement_destroy(&statement);
        return true;
    }
    if (step != DATABASE_STEP_ROW) {
        database_statement_destroy(&statement);
        return false;
    }
    expires = database_column_int64(&statement, 1);
    blob = database_column_blob(&statement, 0, &blob_length);
    if (expires <= (int64_t)time(NULL) ||
        !session_deserialize(context, blob, blob_length)) {
        database_statement_destroy(&statement);
        session_delete_digest(context, cookie);
        return true;
    }
    context->session.token = string_from_in(context->arena, cookie);
    context->session.expires_at = expires;
    context->session.exists = true;
    database_statement_destroy(&statement);
    return context->session.token.data != NULL;
}

static bool session_ensure(Context *context) {
    return context != NULL &&
           (context->session.loaded || session_load(context));
}

bool session_set_string(Context *context, StringView key, StringView value) {
    if (!session_ensure(context) || key.length == 0 || value.data == NULL ||
        !sm_set_string_view(&context->session.values, key, value)) {
        return false;
    }
    context->session.destroy = false;
    context->session.dirty = true;
    return true;
}
bool session_get_string(Context *context, StringView key, StringView *value) {
    return session_ensure(context) &&
           sm_get_string(&context->session.values, key, value);
}
bool session_set_int64(Context *context, StringView key, int64_t value) {
    if (!session_ensure(context) ||
        !sm_set_value(&context->session.values, key, sm_int(value))) {
        return false;
    }
    context->session.destroy = false;
    context->session.dirty = true;
    return true;
}
bool session_get_int64(Context *context, StringView key, int64_t *value) {
    return session_ensure(context) &&
           sm_get_int(&context->session.values, key, value);
}
bool session_set_bool(Context *context, StringView key, bool value) {
    if (!session_ensure(context) ||
        !sm_set_value(&context->session.values, key, sm_bool(value))) {
        return false;
    }
    context->session.destroy = false;
    context->session.dirty = true;
    return true;
}
bool session_get_bool(Context *context, StringView key, bool *value) {
    return session_ensure(context) &&
           sm_get_bool(&context->session.values, key, value);
}
bool session_delete(Context *context, StringView key) {
    if (!session_ensure(context)) {
        return false;
    }
    if (sm_remove(&context->session.values, key)) {
        context->session.dirty = true;
    }
    return true;
}
bool session_clear(Context *context) {
    if (!session_ensure(context)) {
        return false;
    }
    sm_clear(&context->session.values);
    context->session.dirty = true;
    return true;
}
bool session_regenerate(Context *context) {
    if (!session_ensure(context)) {
        return false;
    }
    context->session.regenerate = true;
    context->session.dirty = true;
    return true;
}
bool session_destroy(Context *context) {
    StringView token;

    if (!session_ensure(context)) {
        return false;
    }
    token = string_as_view(&context->session.token);
    if (token.length > 0 && !session_delete_digest(context, token)) {
        return false;
    }
    sm_clear(&context->session.values);
    context->session.destroy = true;
    context->session.regenerate = true;
    context->session.dirty = false;
    context->session.exists = false;
    context_delete_cookie(context, ceasy_session_cookie_name(),
                          (CookieOptions){.path = sv("/"),
                                          .http_only = true,
                                          .same_site = COOKIE_SAME_SITE_LAX,
                                          .secure = ceasy_is_production()});
    return true;
}

bool session_commit(Context *context) {
    String data;
    StringView old_token;
    unsigned char digest[32];
    DatabaseStatement statement = {0};
    int64_t now;
    bool success;

    if (context == NULL || !context->session.loaded ||
        !context->session.dirty) {
        return true;
    }
    if (context->session.destroy) {
        return true;
    }
    old_token = string_as_view(&context->session.token);
    if (context->session.regenerate && old_token.length > 0 &&
        !session_delete_digest(context, old_token)) {
        return false;
    }
    if (old_token.length == 0 || context->session.regenerate) {
        if (!session_generate_token(context)) {
            return false;
        }
    }
    if (!session_serialize(context, &data) || data.length > SESSION_MAX_BYTES ||
        !session_digest(string_as_view(&context->session.token), digest)) {
        return false;
    }
    now = (int64_t)time(NULL);
    context->session.expires_at = now + ceasy_session_ttl_seconds();
    success = database_prepare(
                  context->database, &statement,
                  sv("INSERT OR REPLACE INTO ceasy_sessions "
                     "(token_digest, data, created_at, updated_at, expires_at) "
                     "VALUES (?, ?, COALESCE((SELECT created_at FROM "
                     "ceasy_sessions WHERE token_digest = ?), ?), ?, ?)")) &&
              database_bind_blob(&statement, 1, digest, sizeof(digest)) &&
              database_bind_blob(&statement, 2, data.data, data.length) &&
              database_bind_blob(&statement, 3, digest, sizeof(digest)) &&
              database_bind_int64(&statement, 4, now) &&
              database_bind_int64(&statement, 5, now) &&
              database_bind_int64(&statement, 6, context->session.expires_at) &&
              database_execute(&statement);
    database_statement_destroy(&statement);
    if (!success ||
        !context_set_cookie(
            context, ceasy_session_cookie_name(),
            string_as_view(&context->session.token),
            (CookieOptions){.path = sv("/"),
                            .http_only = true,
                            .same_site = COOKIE_SAME_SITE_LAX,
                            .secure = ceasy_is_production(),
                            .max_age = ceasy_session_ttl_seconds()})) {
        return false;
    }
    context->session.exists = true;
    context->session.dirty = false;
    context->session.regenerate = false;
    return true;
}

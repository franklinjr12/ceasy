#include <ceasy/ceasy.h>

#include <assert.h>
#include <string.h>

int main(void) {
    Arena arena;
    Arena second_arena;
    Arena malformed_arena;
    Database database;
    Context context = {0};
    Context second = {0};
    Context malformed = {0};
    StringView cookie;
    StringView cookie_suffix;
    StringView malformed_cookie;
    StringView cookie_value;
    StringView string_value;
    int64_t integer_value;
    bool bool_value;
    StringView token;

    assert(arena_init(&arena, 4096));
    assert(database_open(&database, ":memory:"));
    assert(database_execute_sql(
        &database,
        sv("CREATE TABLE ceasy_sessions (token_digest BLOB PRIMARY KEY, "
           "data BLOB NOT NULL, created_at INTEGER NOT NULL, "
           "updated_at INTEGER NOT NULL, expires_at INTEGER NOT NULL)")));
    context.arena = &arena;
    context.database = &database;
    assert(csrf_token(&context).length > 0);
    assert(session_set_string(&context, sv("message"), sv("hello")));
    assert(session_set_int64(&context, sv("count"), 42));
    assert(session_set_bool(&context, sv("enabled"), true));
    assert(session_commit(&context));
    assert(context.response_header_count == 1);
    token = context.response_headers[0].value;
    assert(stringv_contains(token, sv("_ceasy_session=")));
    assert(stringv_split_once_char(token, ';', &cookie, &cookie_suffix));

    assert(arena_init(&second_arena, 4096));
    assert(arena_init(&malformed_arena, 4096));
    second.arena = &second_arena;
    second.database = &database;
    second.request.headers[0] =
        (RequestHeader){.name = sv("Cookie"), .value = cookie};
    second.request.header_count = 1;
    assert(session_get_string(&second, sv("message"), &string_value));
    assert(stringv_equal(string_value, sv("hello")));
    assert(session_get_int64(&second, sv("count"), &integer_value));
    assert(integer_value == 42);
    assert(session_get_bool(&second, sv("enabled"), &bool_value));
    assert(bool_value);
    assert(session_regenerate(&second));
    assert(session_commit(&second));
    assert(second.response_header_count == 1);
    cookie_value = second.response_headers[0].value;
    assert(!stringv_equal(cookie_value, token));
    assert(stringv_split_once_char(cookie_value, ';', &malformed_cookie,
                                   &cookie_suffix));
    assert(database_execute_sql(&database,
                                sv("UPDATE ceasy_sessions SET data = X'01'")));
    malformed.arena = &malformed_arena;
    malformed.database = &database;
    malformed.request.headers[0] =
        (RequestHeader){.name = sv("Cookie"), .value = malformed_cookie};
    malformed.request.header_count = 1;
    assert(!session_get_int64(&malformed, sv("count"), &integer_value));
    assert(session_destroy(&second));
    assert(second.response_header_count == 2);
    database_close(&database);
    arena_destroy(&malformed_arena);
    arena_destroy(&second_arena);
    arena_destroy(&arena);
    return 0;
}

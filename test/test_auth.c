#include <ceasy/ceasy.h>

#include <assert.h>

static StringView cookie_pair(StringView set_cookie) {
    StringView pair;
    StringView ignored;

    assert(stringv_split_once_char(set_cookie, ';', &pair, &ignored));
    return pair;
}

static void create_sessions_table(Database *database) {
    assert(database_execute_sql(
        database,
        sv("CREATE TABLE ceasy_sessions (token_digest BLOB PRIMARY KEY, "
           "data BLOB NOT NULL, created_at INTEGER NOT NULL, "
           "updated_at INTEGER NOT NULL, expires_at INTEGER NOT NULL)")));
}

int main(void) {
    Arena first_arena;
    Arena second_arena;
    Arena third_arena;
    Database database;
    Context first = {0};
    Context second = {0};
    Context third = {0};
    StringView first_cookie;
    StringView second_cookie;
    StringView message;
    StringView csrf;
    int64_t user_id;

    assert(arena_init(&first_arena, 4096));
    assert(arena_init(&second_arena, 4096));
    assert(arena_init(&third_arena, 4096));
    assert(database_open(&database, ":memory:"));
    create_sessions_table(&database);
    first.arena = &first_arena;
    first.database = &database;
    csrf = csrf_token(&first);
    assert(csrf.length > 0);
    assert(auth_login(&first, 17));
    assert(auth_signed_in(&first));
    assert(auth_user_id(&first, &user_id));
    assert(user_id == 17);
    assert(flash_set(&first, sv("success"), sv("Welcome.")));
    assert(session_commit(&first));
    assert(first.response_header_count == 1);
    first_cookie = cookie_pair(first.response_headers[0].value);

    second.arena = &second_arena;
    second.database = &database;
    second.request.headers[0] =
        (RequestHeader){.name = sv("Cookie"), .value = first_cookie};
    second.request.header_count = 1;
    assert(csrf_verify(&second, csrf));
    assert(!csrf_verify(&second, sv("invalid-token")));
    assert(auth_signed_in(&second));
    assert(auth_user_id(&second, &user_id));
    assert(user_id == 17);
    message = flash_get(&second, sv("success"));
    assert(stringv_equal(message, sv("Welcome.")));
    assert(flash_get(&second, sv("success")).length == 0);
    assert(session_commit(&second));
    second_cookie = cookie_pair(second.response_headers[0].value);

    third.arena = &third_arena;
    third.database = &database;
    third.request.headers[0] =
        (RequestHeader){.name = sv("Cookie"), .value = second_cookie};
    third.request.header_count = 1;
    assert(!flash_get(&third, sv("success")).length);
    assert(auth_logout(&third));
    assert(!auth_signed_in(&third));
    assert(third.response_header_count == 1);
    assert(stringv_contains(third.response_headers[0].value, sv("Max-Age=0")));

    database_close(&database);
    arena_destroy(&third_arena);
    arena_destroy(&second_arena);
    arena_destroy(&first_arena);
    return 0;
}

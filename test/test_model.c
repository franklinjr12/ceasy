#include <ceasy/ceasy.h>

#include <assert.h>
#include <stddef.h>

typedef struct {
    int64_t id;
    String name;
    int64_t score;
    bool active;
    String created_at;
    String updated_at;
} TestRecord;

static const ModelField test_fields[] = {
    {.name = sv("id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(TestRecord, id),
     .primary_key = true,
     .insertable = false,
     .updatable = false},
    {.name = sv("name"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(TestRecord, name),
     .primary_key = false,
     .insertable = true,
     .updatable = true},
    {.name = sv("score"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(TestRecord, score),
     .primary_key = false,
     .insertable = true,
     .updatable = true},
    {.name = sv("active"),
     .type = MODEL_FIELD_BOOL,
     .offset = offsetof(TestRecord, active),
     .primary_key = false,
     .insertable = true,
     .updatable = true},
    {.name = sv("created_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(TestRecord, created_at),
     .primary_key = false,
     .insertable = false,
     .updatable = false},
    {.name = sv("updated_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(TestRecord, updated_at),
     .primary_key = false,
     .insertable = false,
     .updatable = false},
};

static const ModelDefinition test_definition = {
    .name = sv("TestRecord"),
    .table_name = sv("records"),
    .size = sizeof(TestRecord),
    .fields = test_fields,
    .field_count = sizeof(test_fields) / sizeof(test_fields[0]),
    .find_sql = sv("SELECT id, name, score, active, created_at, updated_at "
                   "FROM records WHERE id = ?"),
    .all_sql = sv("SELECT id, name, score, active, created_at, updated_at "
                  "FROM records ORDER BY id"),
    .insert_sql =
        sv("INSERT INTO records (name, score, active) VALUES (?, ?, ?)"),
    .update_sql = sv("UPDATE records SET name = ?, score = ?, active = ?, "
                     "updated_at = CURRENT_TIMESTAMP WHERE id = ?"),
    .delete_sql = sv("DELETE FROM records WHERE id = ?"),
};

static void test_model_runtime(void) {
    Arena arena;
    Database database;
    Context context = {0};
    TestRecord input = {0};
    TestRecord *found = NULL;
    ModelArray all = {0};

    assert(arena_init(&arena, 128));
    assert(database_open(&database, ":memory:"));
    assert(database_execute_sql(
        &database,
        sv("CREATE TABLE records (id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "name TEXT NOT NULL, score INTEGER NOT NULL, active INTEGER NOT "
           "NULL, "
           "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
           "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)")));
    context.arena = &arena;
    context.database = &database;

    input.name = string_from_in(&arena, sv("Hello'); DROP TABLE records; --"));
    input.score = -9223372036854770000LL;
    input.active = true;
    assert(model_insert(&context, &test_definition, &input));
    assert(input.id > 0);
    assert(!stringv_empty(string_as_view(&input.created_at)));
    assert(!stringv_empty(string_as_view(&input.updated_at)));

    assert(model_find(&context, &test_definition, input.id, (void **)&found) ==
           MODEL_RESULT_OK);
    assert(found != NULL);
    assert(stringv_equal(string_as_view(&found->name),
                         string_as_view(&input.name)));
    assert(found->score == input.score);
    assert(found->active);

    input.name = string_from_in(&arena, sv("updated"));
    input.score = 42;
    input.active = false;
    assert(model_update(&context, &test_definition, &input) == MODEL_RESULT_OK);
    assert(stringv_equal(string_as_view(&input.name), sv("updated")));
    assert(input.score == 42);
    assert(!input.active);

    assert(model_find(&context, &test_definition, 999999, (void **)&found) ==
           MODEL_RESULT_NOT_FOUND);
    assert(found == NULL);
    assert(model_all(&context, &test_definition, &all));
    assert(all.length == 1);
    assert(stringv_equal(string_as_view(&((TestRecord *)all.items)[0].name),
                         sv("updated")));

    for (int index = 0; index < 20; index++) {
        TestRecord row = {0};
        row.name = string_format_in(&arena, "row-%d", index);
        row.score = index;
        row.active = index % 2 != 0;
        assert(model_insert(&context, &test_definition, &row));
    }
    assert(model_all(&context, &test_definition, &all));
    assert(all.length == 21);
    assert(model_destroy(&context, &test_definition, &input) ==
           MODEL_RESULT_OK);
    assert(model_destroy(&context, &test_definition, &input) ==
           MODEL_RESULT_NOT_FOUND);

    assert(database_close(&database));
    arena_destroy(&arena);
}

static void test_model_column_mismatch(void) {
    Arena arena;
    Database database;
    Context context = {0};
    ModelDefinition mismatch = test_definition;
    void *record = NULL;

    assert(arena_init(&arena, 128));
    assert(database_open(&database, ":memory:"));
    assert(database_execute_sql(
        &database,
        sv("CREATE TABLE records (id INTEGER PRIMARY KEY, name TEXT, score "
           "INTEGER, active INTEGER, created_at TEXT, updated_at TEXT)")));
    assert(database_execute_sql(
        &database, sv("INSERT INTO records VALUES (1, 'name', 1, 1, 'created', "
                      "'updated')")));
    context.arena = &arena;
    context.database = &database;
    mismatch.find_sql = sv("SELECT name, id, score, active, created_at, "
                           "updated_at FROM records WHERE id = ?");
    assert(model_find(&context, &mismatch, 1, &record) == MODEL_RESULT_ERROR);
    assert(record == NULL);
    assert(database_close(&database));
    arena_destroy(&arena);
}

int main(void) {
    test_model_runtime();
    test_model_column_mismatch();
    return 0;
}

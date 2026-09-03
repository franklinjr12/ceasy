#include "ceasy/database/database.h"

#include <assert.h>
#include <stdint.h>

int main(void) {
    Database database;
    DatabaseRows rows;

    assert(database_open(&database, ":memory:"));
    assert(database_write(&database,
                          "CREATE TABLE users (id INTEGER, name TEXT)"));
    assert(database_write(&database, "INSERT INTO users VALUES (1, 'Ada')"));
    assert(database_pending_writes(&database) == 2);
    assert(database_read(&database, "SELECT id, name FROM users", &rows));
    assert(database_pending_writes(&database) == 0);
    assert(rows.row_count == 1);
    assert(rows.column_count == 2);
    assert(rows.rows[0][0] != NULL);
    assert(rows.rows[0][1] != NULL);
    assert(rows.rows[0][0][0] == '1');
    assert(rows.rows[0][1][0] == 'A');
    database_rows_free(&rows);
    assert(database_close(&database));

    assert(database_open(&database, ":memory:"));
    assert(database_write(&database,
                          "CREATE TABLE posts (id INTEGER PRIMARY KEY, "
                          "title TEXT NOT NULL, content TEXT NOT NULL)"));
    DatabaseStatement statement = {0};
    assert(database_prepare(
        &database, &statement,
        sv("INSERT INTO posts (title, content) VALUES (?, ?)")));
    assert(database_bind_text(&statement, 1, sv("Hello <script>")));
    assert(database_bind_text(&statement, 2, sv("Body & text")));
    assert(database_execute(&statement));
    assert(database_last_insert_id(&database) == 1);
    assert(database_changes(&database) == 1);
    database_statement_destroy(&statement);
    assert(database_prepare(&database, &statement,
                            sv("SELECT id, title FROM posts WHERE id = ?")));
    assert(database_bind_int64(&statement, 1, 1));
    assert(database_step(&statement) == DATABASE_STEP_ROW);
    assert(database_column_count(&statement) == 2);
    assert(stringv_equal(database_column_name(&statement, 1), sv("title")));
    assert(database_column_int64(&statement, 0) == 1);
    assert(stringv_equal(database_column_text(&statement, 1),
                         sv("Hello <script>")));
    database_statement_destroy(&statement);
    assert(database_close(&database));

    assert(database_open(&database, ":memory:"));
    assert(database_execute_sql(
        &database, sv("CREATE TABLE values_table (value INTEGER)")));
    assert(database_execute_sql(
        &database,
        sv("CREATE TABLE parent (id INTEGER PRIMARY KEY); "
           "CREATE TABLE child (parent_id INTEGER REFERENCES parent(id))")));
    assert(
        database_execute_sql(&database, sv("INSERT INTO parent VALUES (1)")));
    assert(
        !database_execute_sql(&database, sv("INSERT INTO child VALUES (99)")));
    assert(database_begin(&database));
    assert(!database_begin(&database));
    {
        DatabaseStatement transaction_statement = {0};
        assert(database_prepare(&database, &transaction_statement,
                                sv("INSERT INTO values_table VALUES (1)")));
        assert(database_execute(&transaction_statement));
        database_statement_destroy(&transaction_statement);
    }
    assert(database_rollback(&database));
    assert(database_pending_writes(&database) == 0);
    assert(database_begin(&database));
    {
        DatabaseStatement transaction_statement = {0};
        assert(database_prepare(&database, &transaction_statement,
                                sv("INSERT INTO values_table VALUES (2)")));
        assert(database_execute(&transaction_statement));
        database_statement_destroy(&transaction_statement);
    }
    assert(database_commit(&database));
    {
        DatabaseStatement transaction_statement = {0};
        assert(database_prepare(&database, &transaction_statement,
                                sv("SELECT COUNT(*) FROM values_table")));
        assert(database_step(&transaction_statement) == DATABASE_STEP_ROW);
        assert(database_column_int64(&transaction_statement, 0) == 1);
        database_statement_destroy(&transaction_statement);
    }
    assert(database_close(&database));

    return 0;
}

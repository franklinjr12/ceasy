#include "database/database.h"

#include <assert.h>

void test_database(void)
{
    Database database;
    DatabaseRows rows;

    assert(database_open(&database, ":memory:"));
    assert(database_write(&database, "CREATE TABLE users (id INTEGER, name TEXT)"));
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
}

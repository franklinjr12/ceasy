#ifndef CEASY_DATABASE_H
#define CEASY_DATABASE_H

#include <stdbool.h>
#include <stddef.h>

#include <stdint.h>

#include <ceasy/string/string.h>

typedef struct DatabaseWrite DatabaseWrite;
typedef struct Database Database;

typedef struct {
    Database *database;
    void *statement;
} DatabaseStatement;

typedef enum {
    DATABASE_STEP_ERROR = -1,
    DATABASE_STEP_DONE = 0,
    DATABASE_STEP_ROW = 1
} DatabaseStepResult;

typedef struct {
    int column_count;
    char **column_names;
    char ***rows;
    int row_count;
} DatabaseRows;

struct Database {
    void *connection;
    DatabaseWrite *writes;
    size_t write_count;
    size_t write_capacity;
    char error[256];
};

bool database_open(Database *database, const char *path);
/* Flushes queued writes before closing. */
bool database_close(Database *database);

/* Queues SQL. It is executed by database_flush or before database_read. */
bool database_write(Database *database, const char *sql);
bool database_flush(Database *database);
size_t database_pending_writes(const Database *database);

/* Executes trusted SQL immediately. StringView need not be null-terminated. */
bool database_execute_sql(Database *database, StringView sql);

/* Rows and strings belong to caller until database_rows_free. NULL means SQL
 * NULL. */
bool database_read(Database *database, const char *sql, DatabaseRows *rows);
void database_rows_free(DatabaseRows *rows);

const char *database_error(const Database *database);

bool database_prepare(Database *database, DatabaseStatement *statement,
                      StringView sql);
bool database_bind_text(DatabaseStatement *statement, int index,
                        StringView value);
bool database_bind_int64(DatabaseStatement *statement, int index,
                         int64_t value);
bool database_execute(DatabaseStatement *statement);
DatabaseStepResult database_step(DatabaseStatement *statement);
int64_t database_column_int64(DatabaseStatement *statement, int column);
StringView database_column_text(DatabaseStatement *statement, int column);
int database_column_count(DatabaseStatement *statement);
/* Column name borrows SQLite statement storage until finalize. */
StringView database_column_name(DatabaseStatement *statement, int column);
int64_t database_last_insert_id(Database *database);
int64_t database_changes(Database *database);
/* Column text borrows SQLite statement storage until next step/finalize. */
void database_statement_destroy(DatabaseStatement *statement);

#endif

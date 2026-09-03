#include "ceasy/database/database.h"

#include <sqlite3.h>

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct DatabaseWrite {
    char *sql;
};

static sqlite3 *database_connection(const Database *database) {
    return (sqlite3 *)database->connection;
}

static void database_set_error(Database *database, const char *format, ...) {
    va_list args;

    va_start(args, format);
    vsnprintf(database->error, sizeof(database->error), format, args);
    va_end(args);
}

static char *database_copy_text(const char *text, size_t length) {
    char *copy = malloc((size_t)length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, (size_t)length);
    copy[length] = '\0';
    return copy;
}

static void database_clear_writes(Database *database) {
    for (size_t index = 0; index < database->write_count; index++) {
        free(database->writes[index].sql);
    }

    free(database->writes);
    database->writes = NULL;
    database->write_count = 0;
    database->write_capacity = 0;
}

bool database_open(Database *database, const char *path) {
    sqlite3 *connection = NULL;
    int result;
    char *error_message = NULL;

    if (database == NULL || path == NULL) {
        return false;
    }

    memset(database, 0, sizeof(*database));
    result = sqlite3_open(path, &connection);
    if (result != SQLITE_OK) {
        if (connection != NULL) {
            database_set_error(database, "%s", sqlite3_errmsg(connection));
            sqlite3_close(connection);
        } else {
            database_set_error(database, "could not open database");
        }
        return false;
    }

    database->connection = connection;
    result = sqlite3_busy_timeout(connection, 5000);
    if (result != SQLITE_OK ||
        sqlite3_exec(connection, "PRAGMA foreign_keys = ON", NULL, NULL,
                     &error_message) != SQLITE_OK) {
        database_set_error(database, "%s",
                           error_message != NULL ? error_message
                                                 : sqlite3_errmsg(connection));
        sqlite3_free(error_message);
        sqlite3_close(connection);
        database->connection = NULL;
        return false;
    }
    if (strcmp(path, ":memory:") != 0 &&
        sqlite3_exec(connection, "PRAGMA journal_mode = WAL", NULL, NULL,
                     &error_message) != SQLITE_OK) {
        database_set_error(database, "%s",
                           error_message != NULL ? error_message
                                                 : sqlite3_errmsg(connection));
        sqlite3_free(error_message);
        sqlite3_close(connection);
        database->connection = NULL;
        return false;
    }
    return true;
}

bool database_close(Database *database) {
    sqlite3 *connection;
    bool success = true;

    if (database == NULL) {
        return false;
    }

    connection = database_connection(database);
    if (connection != NULL) {
        success = database_flush(database);
        if (sqlite3_close(connection) != SQLITE_OK) {
            database_set_error(database, "%s", sqlite3_errmsg(connection));
            success = false;
        }
    }

    database_clear_writes(database);
    database->connection = NULL;
    return success;
}

bool database_write(Database *database, const char *sql) {
    DatabaseWrite *writes;
    char *sql_copy;

    if (database == NULL || database_connection(database) == NULL ||
        sql == NULL) {
        return false;
    }

    if (database->write_count == database->write_capacity) {
        size_t new_capacity =
            database->write_capacity == 0 ? 8 : database->write_capacity * 2;

        writes = realloc(database->writes, new_capacity * sizeof(*writes));
        if (writes == NULL) {
            database_set_error(database, "out of memory");
            return false;
        }

        database->writes = writes;
        database->write_capacity = new_capacity;
    }

    sql_copy = database_copy_text(sql, strlen(sql));
    if (sql_copy == NULL) {
        database_set_error(database, "out of memory");
        return false;
    }

    database->writes[database->write_count].sql = sql_copy;
    database->write_count++;
    return true;
}

bool database_flush(Database *database) {
    sqlite3 *connection;
    char *sqlite_error = NULL;
    bool own_transaction;

    if (database == NULL) {
        return false;
    }

    connection = database_connection(database);
    if (connection == NULL) {
        return false;
    }

    if (database->write_count == 0) {
        return true;
    }

    own_transaction = !database->in_transaction;
    if (own_transaction && sqlite3_exec(connection, "BEGIN", NULL, NULL,
                                        &sqlite_error) != SQLITE_OK) {
        database_set_error(database, "%s",
                           sqlite_error != NULL ? sqlite_error
                                                : sqlite3_errmsg(connection));
        sqlite3_free(sqlite_error);
        return false;
    }

    for (size_t index = 0; index < database->write_count; index++) {
        if (sqlite3_exec(connection, database->writes[index].sql, NULL, NULL,
                         &sqlite_error) != SQLITE_OK) {
            database_set_error(database, "%s",
                               sqlite_error != NULL
                                   ? sqlite_error
                                   : sqlite3_errmsg(connection));
            sqlite3_free(sqlite_error);
            if (own_transaction)
                sqlite3_exec(connection, "ROLLBACK", NULL, NULL, NULL);
            return false;
        }
    }

    if (own_transaction && sqlite3_exec(connection, "COMMIT", NULL, NULL,
                                        &sqlite_error) != SQLITE_OK) {
        database_set_error(database, "%s",
                           sqlite_error != NULL ? sqlite_error
                                                : sqlite3_errmsg(connection));
        sqlite3_free(sqlite_error);
        sqlite3_exec(connection, "ROLLBACK", NULL, NULL, NULL);
        return false;
    }

    database_clear_writes(database);
    return true;
}

size_t database_pending_writes(const Database *database) {
    return database == NULL ? 0 : database->write_count;
}

bool database_execute_sql(Database *database, StringView sql) {
    sqlite3 *connection;
    char *sql_copy;
    char *sqlite_error = NULL;
    int result;

    if (database == NULL || sql.data == NULL || sql.length == SIZE_MAX) {
        return false;
    }
    connection = database_connection(database);
    if (connection == NULL || !database_flush(database) ||
        sql.length > SIZE_MAX - 1) {
        return false;
    }
    sql_copy = malloc(sql.length + 1);
    if (sql_copy == NULL) {
        database_set_error(database, "out of memory");
        return false;
    }
    if (sql.length > 0) {
        memcpy(sql_copy, sql.data, sql.length);
    }
    sql_copy[sql.length] = '\0';
    result = sqlite3_exec(connection, sql_copy, NULL, NULL, &sqlite_error);
    free(sql_copy);
    if (result != SQLITE_OK) {
        database_set_error(database, "%s",
                           sqlite_error != NULL ? sqlite_error
                                                : sqlite3_errmsg(connection));
        sqlite3_free(sqlite_error);
        return false;
    }
    return true;
}

static bool database_transaction_sql(Database *database, const char *sql,
                                     bool expected_state) {
    sqlite3 *connection;
    char *error = NULL;

    if (database == NULL || (expected_state ? database->in_transaction
                                            : !database->in_transaction)) {
        return false;
    }
    connection = database_connection(database);
    if (connection == NULL || !database_flush(database) ||
        sqlite3_exec(connection, sql, NULL, NULL, &error) != SQLITE_OK) {
        if (database != NULL) {
            database_set_error(database, "%s",
                               error != NULL ? error : "transaction error");
        }
        sqlite3_free(error);
        return false;
    }
    sqlite3_free(error);
    database->in_transaction = expected_state;
    return true;
}

bool database_begin(Database *database) {
    return database_transaction_sql(database, "BEGIN", true);
}
bool database_commit(Database *database) {
    return database_transaction_sql(database, "COMMIT", false);
}
bool database_rollback(Database *database) {
    return database_transaction_sql(database, "ROLLBACK", false);
}

void database_rows_free(DatabaseRows *rows) {
    if (rows == NULL) {
        return;
    }

    for (int row_index = 0; row_index < rows->row_count; row_index++) {
        for (int column_index = 0; column_index < rows->column_count;
             column_index++) {
            free(rows->rows[row_index][column_index]);
        }
        free(rows->rows[row_index]);
    }

    for (int column_index = 0; column_index < rows->column_count;
         column_index++) {
        free(rows->column_names[column_index]);
    }

    free(rows->rows);
    free(rows->column_names);
    memset(rows, 0, sizeof(*rows));
}

bool database_read(Database *database, const char *sql, DatabaseRows *rows) {
    sqlite3 *connection;
    sqlite3_stmt *statement = NULL;
    int result;

    if (database == NULL || rows == NULL || sql == NULL) {
        return false;
    }

    memset(rows, 0, sizeof(*rows));
    connection = database_connection(database);
    if (connection == NULL || !database_flush(database)) {
        return false;
    }

    result = sqlite3_prepare_v2(connection, sql, -1, &statement, NULL);
    if (result != SQLITE_OK) {
        database_set_error(database, "%s", sqlite3_errmsg(connection));
        return false;
    }

    rows->column_count = sqlite3_column_count(statement);
    rows->column_names =
        calloc((size_t)rows->column_count, sizeof(*rows->column_names));
    if (rows->column_count > 0 && rows->column_names == NULL) {
        database_set_error(database, "out of memory");
        sqlite3_finalize(statement);
        database_rows_free(rows);
        return false;
    }

    for (int column_index = 0; column_index < rows->column_count;
         column_index++) {
        rows->column_names[column_index] = database_copy_text(
            sqlite3_column_name(statement, column_index),
            strlen(sqlite3_column_name(statement, column_index)));
        if (rows->column_names[column_index] == NULL) {
            database_set_error(database, "out of memory");
            sqlite3_finalize(statement);
            database_rows_free(rows);
            return false;
        }
    }

    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        char ***new_rows = realloc(rows->rows, (size_t)(rows->row_count + 1) *
                                                   sizeof(*new_rows));
        char **row;

        if (new_rows == NULL) {
            database_set_error(database, "out of memory");
            sqlite3_finalize(statement);
            database_rows_free(rows);
            return false;
        }

        rows->rows = new_rows;
        row = calloc((size_t)rows->column_count, sizeof(*row));
        if (rows->column_count > 0 && row == NULL) {
            database_set_error(database, "out of memory");
            sqlite3_finalize(statement);
            database_rows_free(rows);
            return false;
        }

        rows->rows[rows->row_count] = row;
        for (int column_index = 0; column_index < rows->column_count;
             column_index++) {
            const unsigned char *value =
                sqlite3_column_text(statement, column_index);
            int length = sqlite3_column_bytes(statement, column_index);

            if (value != NULL) {
                row[column_index] =
                    database_copy_text((const char *)value, (size_t)length);
                if (row[column_index] == NULL) {
                    database_set_error(database, "out of memory");
                    sqlite3_finalize(statement);
                    database_rows_free(rows);
                    return false;
                }
            }
        }
        rows->row_count++;
    }

    if (result != SQLITE_DONE) {
        database_set_error(database, "%s", sqlite3_errmsg(connection));
        sqlite3_finalize(statement);
        database_rows_free(rows);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

const char *database_error(const Database *database) {
    return database == NULL ? "invalid database" : database->error;
}

static sqlite3_stmt *database_statement_handle(DatabaseStatement *statement) {
    return statement == NULL ? NULL : (sqlite3_stmt *)statement->statement;
}

bool database_prepare(Database *database, DatabaseStatement *statement,
                      StringView sql) {
    sqlite3_stmt *handle = NULL;
    sqlite3 *connection;
    int result;

    if (database == NULL || statement == NULL || sql.data == NULL ||
        sql.length > (size_t)INT_MAX) {
        return false;
    }
    memset(statement, 0, sizeof(*statement));
    connection = database_connection(database);
    if (connection == NULL || !database_flush(database)) {
        return false;
    }
    result = sqlite3_prepare_v2(connection, sql.data, (int)sql.length, &handle,
                                NULL);
    if (result != SQLITE_OK) {
        database_set_error(database, "%s", sqlite3_errmsg(connection));
        return false;
    }
    statement->database = database;
    statement->statement = handle;
    return true;
}

bool database_bind_text(DatabaseStatement *statement, int index,
                        StringView value) {
    sqlite3_stmt *handle = database_statement_handle(statement);
    int result;

    if (handle == NULL || (value.data == NULL && value.length > 0) ||
        value.length > (size_t)INT_MAX) {
        return false;
    }
    result =
        sqlite3_bind_text(handle, index, value.data != NULL ? value.data : "",
                          (int)value.length, SQLITE_TRANSIENT);
    if (result != SQLITE_OK && statement->database != NULL) {
        database_set_error(
            statement->database, "%s",
            sqlite3_errmsg(database_connection(statement->database)));
    }
    return result == SQLITE_OK;
}

bool database_bind_int64(DatabaseStatement *statement, int index,
                         int64_t value) {
    sqlite3_stmt *handle = database_statement_handle(statement);
    int result;

    if (handle == NULL) {
        return false;
    }
    result = sqlite3_bind_int64(handle, index, value);
    if (result != SQLITE_OK && statement->database != NULL) {
        database_set_error(
            statement->database, "%s",
            sqlite3_errmsg(database_connection(statement->database)));
    }
    return result == SQLITE_OK;
}

bool database_bind_blob(DatabaseStatement *statement, int index,
                        const void *data, size_t length) {
    sqlite3_stmt *handle = database_statement_handle(statement);
    int result;

    if (handle == NULL || (data == NULL && length > 0) ||
        length > (size_t)INT_MAX) {
        return false;
    }
    result =
        sqlite3_bind_blob(handle, index, data, (int)length, SQLITE_TRANSIENT);
    if (result != SQLITE_OK && statement->database != NULL) {
        database_set_error(
            statement->database, "%s",
            sqlite3_errmsg(database_connection(statement->database)));
    }
    return result == SQLITE_OK;
}

bool database_execute(DatabaseStatement *statement) {
    DatabaseStepResult result = database_step(statement);

    return result == DATABASE_STEP_DONE;
}

DatabaseStepResult database_step(DatabaseStatement *statement) {
    sqlite3_stmt *handle = database_statement_handle(statement);
    int result;

    if (handle == NULL) {
        return DATABASE_STEP_ERROR;
    }
    result = sqlite3_step(handle);
    if (result == SQLITE_ROW) {
        return DATABASE_STEP_ROW;
    }
    if (result == SQLITE_DONE) {
        return DATABASE_STEP_DONE;
    }
    if (statement->database != NULL) {
        database_set_error(
            statement->database, "%s",
            sqlite3_errmsg(database_connection(statement->database)));
    }
    return DATABASE_STEP_ERROR;
}

int64_t database_column_int64(DatabaseStatement *statement, int column) {
    sqlite3_stmt *handle = database_statement_handle(statement);

    return handle == NULL ? 0 : sqlite3_column_int64(handle, column);
}

StringView database_column_text(DatabaseStatement *statement, int column) {
    sqlite3_stmt *handle = database_statement_handle(statement);
    const unsigned char *value;

    if (handle == NULL) {
        return (StringView){0};
    }
    value = sqlite3_column_text(handle, column);
    if (value == NULL) {
        return (StringView){0};
    }
    return (StringView){.data = (const char *)value,
                        .length = (size_t)sqlite3_column_bytes(handle, column)};
}

const void *database_column_blob(DatabaseStatement *statement, int column,
                                 size_t *length) {
    sqlite3_stmt *handle = database_statement_handle(statement);
    int bytes;

    if (length != NULL) {
        *length = 0;
    }
    if (handle == NULL) {
        return NULL;
    }
    bytes = sqlite3_column_bytes(handle, column);
    if (length != NULL && bytes >= 0) {
        *length = (size_t)bytes;
    }
    return sqlite3_column_blob(handle, column);
}

int database_column_count(DatabaseStatement *statement) {
    sqlite3_stmt *handle = database_statement_handle(statement);

    return handle == NULL ? 0 : sqlite3_column_count(handle);
}

StringView database_column_name(DatabaseStatement *statement, int column) {
    sqlite3_stmt *handle = database_statement_handle(statement);
    const char *name;

    if (handle == NULL) {
        return (StringView){0};
    }
    name = sqlite3_column_name(handle, column);
    return stringv_from_cstr(name);
}

int64_t database_last_insert_id(Database *database) {
    sqlite3 *connection =
        database == NULL ? NULL : database_connection(database);

    return connection == NULL ? 0
                              : (int64_t)sqlite3_last_insert_rowid(connection);
}

int64_t database_changes(Database *database) {
    sqlite3 *connection =
        database == NULL ? NULL : database_connection(database);

    return connection == NULL ? 0 : (int64_t)sqlite3_changes(connection);
}

void database_statement_destroy(DatabaseStatement *statement) {
    if (statement == NULL) {
        return;
    }
    if (statement->statement != NULL) {
        sqlite3_finalize(database_statement_handle(statement));
    }
    statement->statement = NULL;
    statement->database = NULL;
}

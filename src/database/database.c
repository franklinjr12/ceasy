#include "ceasy/database/database.h"

#include <sqlite3.h>

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

    if (sqlite3_exec(connection, "BEGIN", NULL, NULL, &sqlite_error) !=
        SQLITE_OK) {
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
            sqlite3_exec(connection, "ROLLBACK", NULL, NULL, NULL);
            return false;
        }
    }

    if (sqlite3_exec(connection, "COMMIT", NULL, NULL, &sqlite_error) !=
        SQLITE_OK) {
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

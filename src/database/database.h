#ifndef CEASY_DATABASE_H
#define CEASY_DATABASE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct DatabaseWrite DatabaseWrite;

typedef struct {
    int column_count;
    char **column_names;
    char ***rows;
    int row_count;
} DatabaseRows;

typedef struct {
    void *connection;
    DatabaseWrite *writes;
    size_t write_count;
    size_t write_capacity;
    char error[256];
} Database;

bool database_open(Database *database, const char *path);
/* Flushes queued writes before closing. */
bool database_close(Database *database);

/* Queues SQL. It is executed by database_flush or before database_read. */
bool database_write(Database *database, const char *sql);
bool database_flush(Database *database);
size_t database_pending_writes(const Database *database);

/* Rows and strings belong to caller until database_rows_free. NULL means SQL NULL. */
bool database_read(Database *database, const char *sql, DatabaseRows *rows);
void database_rows_free(DatabaseRows *rows);

const char *database_error(const Database *database);

#endif

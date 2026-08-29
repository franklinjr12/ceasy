#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>

static int run_sql(sqlite3 *database, const char *sql) {
    char *error = NULL;
    int result = sqlite3_exec(database, sql, NULL, NULL, &error);

    if (result != SQLITE_OK) {
        fprintf(stderr, "database error: %s\n",
                error != NULL ? error : sqlite3_errmsg(database));
        sqlite3_free(error);
    }

    return result;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "db/development.sqlite3";
    sqlite3 *database = NULL;
    int result;

    result = sqlite3_open(path, &database);
    if (result != SQLITE_OK) {
        fprintf(stderr, "could not open %s: %s\n", path,
                database != NULL ? sqlite3_errmsg(database) : "unknown error");
        sqlite3_close(database);
        return EXIT_FAILURE;
    }

    result =
        run_sql(database, "CREATE TABLE IF NOT EXISTS posts ("
                          "id INTEGER PRIMARY KEY,"
                          "title TEXT NOT NULL,"
                          "content TEXT NOT NULL,"
                          "created_at TEXT NOT NULL,"
                          "updated_at TEXT NOT NULL"
                          ");"
                          "BEGIN;"
                          "INSERT OR IGNORE INTO posts "
                          "(id, title, content, created_at, updated_at) VALUES "
                          "(1, 'Welcome to Ceasy', 'Ceasy is a small web "
                          "framework written in C.', "
                          "CURRENT_TIMESTAMP, CURRENT_TIMESTAMP);"
                          "INSERT OR IGNORE INTO posts "
                          "(id, title, content, created_at, updated_at) VALUES "
                          "(2, 'SQLite for Persistence', 'SQLite gives Ceasy a "
                          "simple local database.', "
                          "CURRENT_TIMESTAMP, CURRENT_TIMESTAMP);"
                          "INSERT OR IGNORE INTO posts "
                          "(id, title, content, created_at, updated_at) VALUES "
                          "(3, 'Hello from the Seed Tool', 'This post was "
                          "created by tools/seed_database.', "
                          "CURRENT_TIMESTAMP, CURRENT_TIMESTAMP);"
                          "COMMIT;");
    sqlite3_close(database);

    if (result != SQLITE_OK) {
        return EXIT_FAILURE;
    }

    printf("seeded %s\n", path);
    return EXIT_SUCCESS;
}

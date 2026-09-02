#include "migration.h"

#include <ceasy/ceasy.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb");

    assert(file != NULL);
    assert(fwrite(contents, 1, strlen(contents), file) == strlen(contents));
    assert(fclose(file) == 0);
}

static void test_migrations(void) {
    char root_template[] = "/tmp/ceasy-migration-test-XXXXXX";
    char *root = mkdtemp(root_template);
    char path[PATH_MAX];
    CeasyProject project = {0};
    Database database;
    DatabaseRows rows;

    assert(root != NULL);
    assert(strlen(root) < sizeof(project.root));
    strcpy(project.root, root);
    assert(snprintf(path, sizeof(path), "%s/db", root) > 0);
    assert(mkdir(path, 0755) == 0);
    assert(snprintf(path, sizeof(path), "%s/db/migrations", root) > 0);
    assert(mkdir(path, 0755) == 0);
    assert(snprintf(path, sizeof(path), "%s/cdev.conf", root) > 0);
    write_file(path, "name fixture\n");
    assert(snprintf(path, sizeof(path),
                    "%s/db/migrations/"
                    "20260101000002_add_second.sql",
                    root) > 0);
    write_file(path, "INSERT INTO users (id) VALUES (2);\n");
    assert(snprintf(path, sizeof(path),
                    "%s/db/migrations/"
                    "20260101000001_add_first.sql",
                    root) > 0);
    write_file(path, "INSERT INTO users (id) VALUES (1);\n");
    assert(snprintf(path, sizeof(path),
                    "%s/db/migrations/"
                    "20260101000000_create_users.sql",
                    root) > 0);
    write_file(path, "CREATE TABLE users (id INTEGER PRIMARY KEY);\n");

    assert(migration_run(&project));
    assert(snprintf(path, sizeof(path), "%s/db/development.sqlite3", root) > 0);
    assert(database_open(&database, path));
    assert(database_read(&database, "SELECT version FROM schema_migrations",
                         &rows));
    assert(rows.row_count == 3);
    database_rows_free(&rows);
    assert(database_read(&database, "SELECT id FROM users ORDER BY id", &rows));
    assert(rows.row_count == 2);
    assert(rows.rows[0][0][0] == '1');
    assert(rows.rows[1][0][0] == '2');
    database_rows_free(&rows);
    assert(database_close(&database));
    assert(migration_run(&project));

    assert(snprintf(path, sizeof(path),
                    "%s/db/migrations/"
                    "20260101000003_bad.sql",
                    root) > 0);
    write_file(path, "CREATE TABLE temporary (id INTEGER);\nINVALID SQL;\n");
    assert(!migration_run(&project));
    assert(snprintf(path, sizeof(path), "%s/db/development.sqlite3", root) > 0);
    assert(database_open(&database, path));
    assert(database_read(
        &database, "SELECT name FROM sqlite_master WHERE name = 'temporary'",
        &rows));
    assert(rows.row_count == 0);
    database_rows_free(&rows);
    assert(database_read(&database, "SELECT version FROM schema_migrations",
                         &rows));
    assert(rows.row_count == 3);
    database_rows_free(&rows);
    assert(database_close(&database));

    assert(snprintf(path, sizeof(path),
                    "%s/db/migrations/"
                    "20260101000003_bad.sql",
                    root) > 0);
    assert(unlink(path) == 0);
    assert(snprintf(path, sizeof(path),
                    "%s/db/migrations/"
                    "20260101000000_create_users.sql",
                    root) > 0);
    assert(unlink(path) == 0);
    assert(snprintf(path, sizeof(path),
                    "%s/db/migrations/"
                    "20260101000001_add_first.sql",
                    root) > 0);
    assert(unlink(path) == 0);
    assert(snprintf(path, sizeof(path),
                    "%s/db/migrations/"
                    "20260101000002_add_second.sql",
                    root) > 0);
    assert(unlink(path) == 0);
    assert(snprintf(path, sizeof(path), "%s/cdev.conf", root) > 0);
    assert(unlink(path) == 0);
    assert(snprintf(path, sizeof(path), "%s/db/development.sqlite3", root) > 0);
    assert(unlink(path) == 0);
    assert(snprintf(path, sizeof(path), "%s/db/migrations", root) > 0);
    assert(rmdir(path) == 0);
    assert(snprintf(path, sizeof(path), "%s/db", root) > 0);
    assert(rmdir(path) == 0);
    assert(rmdir(root) == 0);
}

int main(void) {
    test_migrations();
    return 0;
}

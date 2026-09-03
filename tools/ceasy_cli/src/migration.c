#include "migration.h"

#include <ceasy/ceasy.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MIGRATION_LIMIT (16u * 1024u * 1024u)

typedef struct {
    char name[NAME_MAX];
    char version[15];
    char path[PATH_MAX];
} MigrationFile;

static bool migration_version_valid(const char *name, char *version,
                                    size_t version_size) {
    size_t length = strlen(name);
    bool separator = length > 14 && name[14] == '_';

    if (!separator || length < 20 || strcmp(name + length - 4, ".sql") != 0 ||
        version_size < 15) {
        return false;
    }
    for (size_t index = 0; index < 14; index++) {
        if (name[index] < '0' || name[index] > '9') {
            return false;
        }
    }
    for (size_t index = 15; index < length - 4; index++) {
        if (!((name[index] >= 'a' && name[index] <= 'z') ||
              (name[index] >= '0' && name[index] <= '9') ||
              name[index] == '_')) {
            return false;
        }
    }
    memcpy(version, name, 14);
    version[14] = '\0';
    return true;
}

static int migration_compare(const void *left, const void *right) {
    const MigrationFile *a = left;
    const MigrationFile *b = right;

    return strcmp(a->name, b->name);
}

static bool migration_collect(const CeasyProject *project,
                              MigrationFile **items, size_t *count) {
    char directory_path[PATH_MAX];
    DIR *directory;
    MigrationFile *migrations = NULL;
    size_t migration_count = 0;
    size_t capacity = 0;
    struct dirent *entry;

    if (!project_path(project, "db/migrations", directory_path,
                      sizeof(directory_path))) {
        return false;
    }
    directory = opendir(directory_path);
    if (directory == NULL) {
        if (errno == ENOENT) {
            *items = NULL;
            *count = 0;
            return true;
        }
        return false;
    }
    while ((entry = readdir(directory)) != NULL) {
        MigrationFile *replacement;
        MigrationFile migration;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            !stringv_ends_with(stringv_from_cstr(entry->d_name), sv(".sql"))) {
            continue;
        }
        memset(&migration, 0, sizeof(migration));
        if (strlen(entry->d_name) >= sizeof(migration.name) ||
            !migration_version_valid(entry->d_name, migration.version,
                                     sizeof(migration.version))) {
            fprintf(stderr, "error: invalid migration filename '%s'\n",
                    entry->d_name);
            closedir(directory);
            free(migrations);
            return false;
        }
        strcpy(migration.name, entry->d_name);
        if (!project_path(project, "db/migrations", migration.path,
                          sizeof(migration.path)) ||
            strlen(migration.path) + strlen(entry->d_name) + 2 >
                sizeof(migration.path)) {
            closedir(directory);
            free(migrations);
            return false;
        }
        strcat(migration.path, "/");
        strcat(migration.path, entry->d_name);
        if (migration_count == capacity) {
            size_t new_capacity = capacity == 0 ? 8 : capacity * 2;

            replacement =
                realloc(migrations, new_capacity * sizeof(*replacement));
            if (replacement == NULL) {
                closedir(directory);
                free(migrations);
                return false;
            }
            migrations = replacement;
            capacity = new_capacity;
        }
        migrations[migration_count++] = migration;
    }
    closedir(directory);
    qsort(migrations, migration_count, sizeof(*migrations), migration_compare);
    *items = migrations;
    *count = migration_count;
    return true;
}

static char *migration_read(const MigrationFile *migration) {
    FILE *file = fopen(migration->path, "rb");
    long size;
    char *contents;

    if (file == NULL || fseek(file, 0, SEEK_END) != 0 ||
        (size = ftell(file)) < 0 || (unsigned long)size > MIGRATION_LIMIT ||
        fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return NULL;
    }
    contents = malloc((size_t)size + 1);
    if (contents == NULL ||
        fread(contents, 1, (size_t)size, file) != (size_t)size) {
        free(contents);
        fclose(file);
        return NULL;
    }
    fclose(file);
    contents[size] = '\0';
    return contents;
}

static int migration_applied(Database *database, const char *version) {
    DatabaseStatement statement = {0};
    DatabaseStepResult step;

    if (!database_prepare(
            database, &statement,
            sv("SELECT 1 FROM schema_migrations WHERE version = ?")) ||
        !database_bind_text(&statement, 1, stringv_from_cstr(version))) {
        database_statement_destroy(&statement);
        return -1;
    }
    step = database_step(&statement);
    database_statement_destroy(&statement);
    if (step == DATABASE_STEP_ROW) {
        return 1;
    }
    return step == DATABASE_STEP_DONE ? 0 : -1;
}

static bool migration_record(Database *database, const char *version) {
    DatabaseStatement statement = {0};
    bool success =
        database_prepare(
            database, &statement,
            sv("INSERT INTO schema_migrations (version) VALUES (?)")) &&
        database_bind_text(&statement, 1, stringv_from_cstr(version)) &&
        database_execute(&statement);

    database_statement_destroy(&statement);
    return success;
}

bool migration_run(const CeasyProject *project) {
    MigrationFile *migrations = NULL;
    size_t migration_count = 0;
    char database_path[PATH_MAX];
    Database database;
    size_t applied = 0;
    char db_directory[PATH_MAX];

    if (!project_path(project, "db", db_directory, sizeof(db_directory))) {
        return false;
    }
    if (mkdir(db_directory, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "error: cannot create db directory\n");
        return false;
    }

    if (!migration_collect(project, &migrations, &migration_count)) {
        fprintf(stderr, "error: cannot initialize migration database\n");
        free(migrations);
        return false;
    }
    {
        const char *configured_path = getenv("CEASY_DATABASE_PATH");
        bool has_configured_path =
            configured_path != NULL && configured_path[0] != '\0';

        if (has_configured_path &&
            strlen(configured_path) < sizeof(database_path)) {
            memcpy(database_path, configured_path, strlen(configured_path) + 1);
        }
        if ((has_configured_path &&
             strlen(configured_path) >= sizeof(database_path)) ||
            (!has_configured_path &&
             !project_path(project, "db/development.sqlite3", database_path,
                           sizeof(database_path))) ||
            !database_open(&database, database_path)) {
            fprintf(stderr, "error: cannot open migration database\n");
            free(migrations);
            return false;
        }
    }
    if (!database_execute_sql(&database,
                              sv("CREATE TABLE IF NOT EXISTS schema_migrations "
                                 "(version TEXT PRIMARY KEY, applied_at TEXT "
                                 "NOT NULL DEFAULT CURRENT_TIMESTAMP)"))) {
        fprintf(stderr, "error: cannot initialize migration database: %s\n",
                database_error(&database));
        database_close(&database);
        free(migrations);
        return false;
    }
    for (size_t index = 0; index < migration_count; index++) {
        int is_applied =
            migration_applied(&database, migrations[index].version);
        char *sql;

        if (is_applied < 0) {
            fprintf(stderr, "error: cannot inspect schema_migrations: %s\n",
                    database_error(&database));
            database_close(&database);
            free(migrations);
            return false;
        }
        if (is_applied != 0) {
            continue;
        }
        sql = migration_read(&migrations[index]);
        printf("migrate %.*s\n", (int)(strlen(migrations[index].name) - 4),
               migrations[index].name);
        if (sql == NULL || !database_execute_sql(&database, sv("BEGIN")) ||
            !database_execute_sql(&database, stringv_from_cstr(sql)) ||
            !migration_record(&database, migrations[index].version) ||
            !database_execute_sql(&database, sv("COMMIT"))) {
            database_execute_sql(&database, sv("ROLLBACK"));
            fprintf(stderr, "error: migration %s failed: %s\n",
                    migrations[index].name,
                    sql == NULL ? "could not read migration"
                                : database_error(&database));
            free(sql);
            database_close(&database);
            free(migrations);
            return false;
        }
        free(sql);
        applied++;
    }
    if (applied == 0) {
        printf("database is up to date\n");
    } else {
        printf("migrated %zu migration%s\n", applied, applied == 1 ? "" : "s");
    }
    bool close_success = database_close(&database);
    free(migrations);
    return close_success;
}

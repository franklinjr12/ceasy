#include "sessions.h"

#include <ceasy/ceasy.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static bool sessions_timestamp(char *value, size_t size) {
    time_t now = time(NULL);
    struct tm local;

    return now != (time_t)-1 && localtime_r(&now, &local) != NULL &&
           strftime(value, size, "%Y%m%d%H%M%S", &local) == 14;
}

bool sessions_install(const CeasyProject *project) {
    char directory[PATH_MAX];
    char relative[PATH_MAX];
    char path[PATH_MAX];
    char timestamp[32];
    FILE *file;
    int written;

    if (!project_path(project, "db/migrations", directory, sizeof(directory)))
        return false;
    if (mkdir(directory, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "error: cannot create db/migrations\n");
        return false;
    }
    if (!sessions_timestamp(timestamp, sizeof(timestamp))) {
        return false;
    }
    written = snprintf(relative, sizeof(relative),
                       "db/migrations/%s_create_ceasy_sessions.sql", timestamp);
    if (written < 0 || (size_t)written >= sizeof(relative)) {
        return false;
    }
    written = snprintf(path, sizeof(path), "%s/%s", project->root, relative);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        return false;
    }
    if (access(path, F_OK) == 0) {
        fprintf(stderr, "error: %s already exists\n", relative);
        return false;
    }
    file = fopen(path, "wb");
    if (file == NULL ||
        fputs("CREATE TABLE IF NOT EXISTS ceasy_sessions (\n"
              "    token_digest BLOB PRIMARY KEY,\n"
              "    data BLOB NOT NULL,\n"
              "    created_at INTEGER NOT NULL,\n"
              "    updated_at INTEGER NOT NULL,\n"
              "    expires_at INTEGER NOT NULL\n"
              ");\n"
              "CREATE INDEX IF NOT EXISTS ceasy_sessions_expires_at_idx\n"
              "    ON ceasy_sessions(expires_at);\n",
              file) == EOF) {
        if (file != NULL) {
            fclose(file);
        }
        unlink(path);
        fprintf(stderr, "error: cannot write %s\n", relative);
        return false;
    }
    if (fclose(file) != 0) {
        unlink(path);
        fprintf(stderr, "error: cannot write %s\n", relative);
        return false;
    }
    printf("create %s\n", relative);
    return true;
}

bool sessions_cleanup(const CeasyProject *project) {
    char path[PATH_MAX];
    const char *configured_path = getenv("CEASY_DATABASE_PATH");
    Database database;
    DatabaseStatement statement = {0};
    bool success;
    time_t now = time(NULL);

    if (now == (time_t)-1 || configured_path == NULL ||
        configured_path[0] == '\0') {
        configured_path = NULL;
    }
    if (configured_path != NULL && strlen(configured_path) < sizeof(path)) {
        memcpy(path, configured_path, strlen(configured_path) + 1);
    }
    if ((configured_path != NULL && strlen(configured_path) >= sizeof(path)) ||
        (configured_path == NULL &&
         !project_path(project, "db/development.sqlite3", path,
                       sizeof(path))) ||
        !database_open(&database, path)) {
        fprintf(stderr, "error: cannot open session database\n");
        return false;
    }
    success = database_prepare(
                  &database, &statement,
                  sv("DELETE FROM ceasy_sessions WHERE expires_at <= ?")) &&
              database_bind_int64(&statement, 1, (int64_t)now) &&
              database_execute(&statement);
    if (!success)
        fprintf(stderr, "error: session cleanup failed: %s\n",
                database_error(&database));
    database_statement_destroy(&statement);
    database_close(&database);
    return success;
}

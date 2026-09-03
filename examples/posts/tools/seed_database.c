#include <sqlite3.h>
#include <sodium.h>

#include <stdio.h>
#include <stdlib.h>

static int run_sql(sqlite3 *database, const char *sql) {
    char *error = NULL;
    int result = sqlite3_exec(database, sql, NULL, NULL, &error);
    if (result != SQLITE_OK) {
        fprintf(stderr, "database error: %s\n", error != NULL ? error : sqlite3_errmsg(database));
        sqlite3_free(error);
    }
    return result;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "db/development.sqlite3";
    sqlite3 *database = NULL;
    char admin_hash[crypto_pwhash_STRBYTES];
    char reader_hash[crypto_pwhash_STRBYTES];
    char *sql;
    int result;

    if (sodium_init() < 0 ||
        crypto_pwhash_str(admin_hash, "ceasy-development", 17,
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0 ||
        crypto_pwhash_str(reader_hash, "reader-development", 18,
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0 ||
        sqlite3_open(path, &database) != SQLITE_OK) {
        fprintf(stderr, "could not initialize seed database %s\n", path);
        sqlite3_close(database);
        return EXIT_FAILURE;
    }
    sql = sqlite3_mprintf(
        "UPDATE users SET password_digest=%Q WHERE email='admin@ceasy.local';"
        "INSERT OR IGNORE INTO users (name,email,password_digest,bio,is_admin) "
        "VALUES ('Ada Reader','ada@ceasy.local',%Q,'Learning in public.',0);"
        "INSERT OR REPLACE INTO posts (id,user_id,title,summary,content,published,published_at) "
        "VALUES (1,1,'Welcome to Ceasy Journal','A small publishing application written entirely in C.','Ceasy Journal is a small publishing application written entirely in C. It is a working example of server-rendered pages, SQLite, and secure sessions.',1,CURRENT_TIMESTAMP);"
        "INSERT OR REPLACE INTO posts (id,user_id,title,summary,content,published,published_at) "
        "VALUES (2,1,'SQLite for Persistence','Simple storage can still have production-minded defaults.','SQLite gives Ceasy a simple local database while prepared statements, foreign keys, WAL, and busy timeouts keep the boundary honest.',1,CURRENT_TIMESTAMP);"
        "INSERT OR REPLACE INTO posts (id,user_id,title,summary,content,published,published_at) "
        "VALUES (3,2,'Hello from the Community','Notes from a reader building with Ceasy.','This article was created by the development seed and demonstrates that posts belong to users.',1,CURRENT_TIMESTAMP);"
        "INSERT OR IGNORE INTO posts (user_id,title,summary,content,published,published_at) "
        "VALUES (1,'An unfinished idea','A private draft for the dashboard.','Drafts stay private until their author publishes them.',0,'');",
        admin_hash, reader_hash);
    result = sql == NULL ? SQLITE_NOMEM : run_sql(database, sql);
    sqlite3_free(sql);
    sqlite3_close(database);
    if (result != SQLITE_OK) return EXIT_FAILURE;
    printf("seeded %s\n", path);
    return EXIT_SUCCESS;
}

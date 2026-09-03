#include <ceasy/config/config.h>

#include <assert.h>
#include <stdlib.h>

int main(void) {
    setenv("CEASY_ENV", "production", 1);
    setenv("CEASY_PORT", "8443", 1);
    setenv("CEASY_DATABASE_PATH", "/data/blog.sqlite3", 1);
    setenv("CEASY_SESSION_COOKIE", "journal_session", 1);
    setenv("CEASY_SESSION_TTL_SECONDS", "90", 1);
    assert(ceasy_config_init());
    assert(ceasy_is_production());
    assert(ceasy_port() == 8443);
    assert(stringv_equal(ceasy_database_path(), sv("/data/blog.sqlite3")));
    assert(stringv_equal(ceasy_session_cookie_name(), sv("journal_session")));
    assert(ceasy_session_ttl_seconds() == 90);

    setenv("CEASY_PORT", "70000", 1);
    assert(!ceasy_config_init());
    assert(ceasy_config_error()[0] != '\0');

    setenv("CEASY_ENV", "development", 1);
    setenv("CEASY_PORT", "3000", 1);
    setenv("CEASY_DATABASE_PATH", "db/development.sqlite3", 1);
    setenv("CEASY_SESSION_COOKIE", "_ceasy_session", 1);
    setenv("CEASY_SESSION_TTL_SECONDS", "604800", 1);
    assert(ceasy_config_init());
    assert(ceasy_is_development());
    return 0;
}

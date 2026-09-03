#include <ceasy/config/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool initialized;
    bool valid;
    CeasyEnvironment environment;
    uint16_t port;
    int64_t ttl;
    StringView database_path;
    StringView cookie_name;
    char error[256];
} CeasyConfig;

static CeasyConfig config;

static const char *config_env(const char *name, const char *fallback) {
    const char *value = getenv(name);

    return value == NULL || value[0] == '\0' ? fallback : value;
}

static bool config_parse_environment(const char *value,
                                     CeasyEnvironment *environment) {
    if (strcmp(value, "development") == 0) {
        *environment = CEASY_ENV_DEVELOPMENT;
        return true;
    }
    if (strcmp(value, "test") == 0) {
        *environment = CEASY_ENV_TEST;
        return true;
    }
    if (strcmp(value, "production") == 0) {
        *environment = CEASY_ENV_PRODUCTION;
        return true;
    }
    return false;
}

static bool config_parse_integer(const char *name, const char *value,
                                 int64_t minimum, int64_t maximum,
                                 int64_t *result) {
    char *end = NULL;
    long long parsed;

    if (value[0] == '\0') {
        snprintf(config.error, sizeof(config.error), "%s is empty", name);
        return false;
    }
    parsed = strtoll(value, &end, 10);
    if (end == value || *end != '\0' || parsed < minimum || parsed > maximum) {
        snprintf(config.error, sizeof(config.error), "invalid %s: %s", name,
                 value);
        return false;
    }
    *result = (int64_t)parsed;
    return true;
}

bool ceasy_config_init(void) {
    const char *environment = config_env("CEASY_ENV", "development");
    const char *port = config_env("CEASY_PORT", "3000");
    const char *database_path =
        config_env("CEASY_DATABASE_PATH", "db/development.sqlite3");
    const char *cookie = config_env("CEASY_SESSION_COOKIE", "_ceasy_session");
    const char *ttl = config_env("CEASY_SESSION_TTL_SECONDS", "604800");
    int64_t parsed;

    memset(&config, 0, sizeof(config));
    config.initialized = true;
    if (!config_parse_environment(environment, &config.environment) ||
        !config_parse_integer("CEASY_PORT", port, 1, 65535, &parsed)) {
        if (config.error[0] == '\0') {
            snprintf(config.error, sizeof(config.error),
                     "invalid CEASY_ENV: %s", environment);
        }
        return false;
    }
    config.port = (uint16_t)parsed;
    if (!config_parse_integer("CEASY_SESSION_TTL_SECONDS", ttl, 1, INT64_MAX,
                              &config.ttl)) {
        return false;
    }
    if (cookie[0] == '\0' || strlen(cookie) > 128 ||
        strchr(cookie, '=') != NULL || strchr(cookie, ';') != NULL) {
        snprintf(config.error, sizeof(config.error),
                 "invalid CEASY_SESSION_COOKIE");
        return false;
    }
    config.database_path = stringv_from_cstr(database_path);
    config.cookie_name = stringv_from_cstr(cookie);
    config.valid = true;
    return true;
}

const char *ceasy_config_error(void) {
    return config.error[0] == '\0' ? "invalid configuration" : config.error;
}

static bool config_ready(void) { return config.initialized && config.valid; }
CeasyEnvironment ceasy_environment(void) {
    return config_ready() ? config.environment : CEASY_ENV_DEVELOPMENT;
}
bool ceasy_is_development(void) {
    return ceasy_environment() == CEASY_ENV_DEVELOPMENT;
}
bool ceasy_is_test(void) { return ceasy_environment() == CEASY_ENV_TEST; }
bool ceasy_is_production(void) {
    return ceasy_environment() == CEASY_ENV_PRODUCTION;
}
StringView ceasy_database_path(void) {
    return config_ready() ? config.database_path : sv("db/development.sqlite3");
}
uint16_t ceasy_port(void) { return config_ready() ? config.port : 3000; }
StringView ceasy_session_cookie_name(void) {
    return config_ready() ? config.cookie_name : sv("_ceasy_session");
}
int64_t ceasy_session_ttl_seconds(void) {
    return config_ready() ? config.ttl : 604800;
}

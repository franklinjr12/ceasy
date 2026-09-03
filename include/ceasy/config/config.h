#ifndef CEASY_CONFIG_H
#define CEASY_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include <ceasy/string/string.h>

typedef enum {
    CEASY_ENV_DEVELOPMENT,
    CEASY_ENV_TEST,
    CEASY_ENV_PRODUCTION
} CeasyEnvironment;

bool ceasy_config_init(void);
const char *ceasy_config_error(void);
CeasyEnvironment ceasy_environment(void);
bool ceasy_is_development(void);
bool ceasy_is_test(void);
bool ceasy_is_production(void);
StringView ceasy_database_path(void);
uint16_t ceasy_port(void);
StringView ceasy_session_cookie_name(void);
int64_t ceasy_session_ttl_seconds(void);

#endif

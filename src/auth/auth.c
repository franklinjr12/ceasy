#include <ceasy/auth/auth.h>

#include <ceasy/session/session.h>

#define AUTH_USER_KEY "__ceasy.auth.user_id"

bool auth_login(Context *context, int64_t user_id) {
    return user_id > 0 && session_regenerate(context) &&
           session_set_int64(context, sv(AUTH_USER_KEY), user_id);
}
bool auth_logout(Context *context) { return session_destroy(context); }
bool auth_signed_in(Context *context) {
    int64_t user_id;
    return auth_user_id(context, &user_id);
}
bool auth_user_id(Context *context, int64_t *user_id) {
    return user_id != NULL &&
           session_get_int64(context, sv(AUTH_USER_KEY), user_id) &&
           *user_id > 0;
}

#include "posts_controller.h"

void routes(Router *router) {
    route_get(router, "/", home_index);
    route_get(router, "/posts", posts_index);
    router_route_get_guarded(router, "/posts/new", require_authenticated_user,
                             posts_new);
    router_route_post_guarded(router, "/posts", require_authenticated_user,
                              posts_create);
    route_get(router, "/posts/:id", posts_show);
    router_route_get_guarded(router, "/posts/:id/edit",
                             require_authenticated_user, posts_edit);
    router_route_patch_guarded(router, "/posts/:id", require_authenticated_user,
                               posts_update);
    router_route_delete_guarded(router, "/posts/:id",
                                require_authenticated_user, posts_destroy);
    route_post(router, "/posts/:id/comments", comments_create);
    route_delete(router, "/comments/:id", comments_destroy);
    route_get(router, "/authors/:id", authors_show);
    route_get(router, "/signup", users_new);
    route_post(router, "/signup", users_create);
    route_get(router, "/login", sessions_new);
    route_post(router, "/login", sessions_create);
    route_delete(router, "/logout", sessions_destroy);
    router_route_get_guarded(router, "/dashboard", require_authenticated_user,
                             dashboard_index);
    router_route_get_guarded(router, "/account", require_authenticated_user,
                             account_edit);
    router_route_patch_guarded(router, "/account", require_authenticated_user,
                               account_update);
    router_route_patch_guarded(router, "/account/password",
                               require_authenticated_user,
                               account_password_update);
    route_get(router, "/health", health_index);
    route_get(router, "/ready", ready_index);
}

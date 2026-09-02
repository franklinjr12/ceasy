#include "posts_controller.h"

void routes(Router *router) {
    route_get(router, "/", posts_index);
    route_get(router, "/posts", posts_index);
    route_get(router, "/posts/new", posts_new);
    route_post(router, "/posts", posts_create);
    route_get(router, "/posts/:id", posts_show);
    route_get(router, "/posts/:id/edit", posts_edit);
    route_patch(router, "/posts/:id", posts_update);
    route_delete(router, "/posts/:id", posts_destroy);
}

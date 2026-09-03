#include "ceasy/ceasy.h"

#include <assert.h>
#include <string.h>
static int handler_calls;
static int guard_calls;

static void posts_show(Context *context) {
    assert(context != NULL);
    assert(stringv_equal(context->request.path, sv("/posts/42")));
    assert(stringv_equal(context_param(context, sv("id")), sv("42")));
    handler_calls++;
}

static void posts_new(Context *context) {
    assert(context != NULL);
    handler_calls++;
}

static void nested_show(Context *context) {
    assert(stringv_equal(context_param(context, sv("user_id")), sv("7")));
    assert(stringv_equal(context_param(context, sv("id")), sv("42")));
    handler_calls++;
}

static bool deny_guard(Context *context) {
    assert(context != NULL);
    guard_calls++;
    return false;
}

int main(void) {
    Router router;
    Context context;

    router_init(&router);
    memset(&context, 0, sizeof(context));
    assert(route_get(&router, "/posts/:id", posts_show));
    assert(route_get(&router, "/posts/new", posts_new));
    assert(route_post(&router, "/posts", posts_new));
    assert(route_patch(&router, "/posts/:id", posts_new));
    assert(route_delete(&router, "/posts/:id", posts_new));
    assert(route_get(&router, "/users/:user_id/posts/:id", nested_show));
    assert(
        router_route_get_guarded(&router, "/private", deny_guard, posts_new));

    assert(router_dispatch(&router, "GET", "/posts/42", &context) ==
           ROUTER_DISPATCHED);
    assert(handler_calls == 1);
    assert(router_dispatch(&router, "GET", "/posts/new", &context) ==
           ROUTER_DISPATCHED);
    assert(handler_calls == 2);
    assert(router_dispatch(&router, "GET", "/users/7/posts/42", &context) ==
           ROUTER_DISPATCHED);
    assert(handler_calls == 3);
    assert(router_dispatch(&router, "PUT", "/posts", &context) ==
           ROUTER_METHOD_NOT_ALLOWED);
    assert(router_dispatch(&router, "GET", "/missing", &context) ==
           ROUTER_NOT_FOUND);
    assert(router_dispatch(&router, "PATCH", "/posts/42", &context) ==
           ROUTER_DISPATCHED);
    assert(router_dispatch(&router, "DELETE", "/posts/42", &context) ==
           ROUTER_DISPATCHED);
    assert(router_dispatch(&router, "GET", "/private", &context) ==
           ROUTER_DISPATCHED);
    assert(guard_calls == 1);
    assert(handler_calls == 5);

    router_reset(router_default());
    assert(route_get("/health", posts_new));
    assert(router_dispatch(router_default(), "GET", "/health", &context) ==
           ROUTER_DISPATCHED);

    return 0;
}

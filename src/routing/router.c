#include "ceasy/routing/router.h"
#include "ceasy/context.h"

#include <string.h>

static Router default_router;

static bool router_valid_path(const char *path) {
    return path != NULL && path[0] == '/';
}

static bool router_next_segment(StringView path, size_t *cursor,
                                StringView *segment) {
    size_t start = *cursor;
    size_t relative_end;

    if (start < path.length && path.data[start] == '/') {
        start++;
    }
    if (start >= path.length) {
        return false;
    }
    if (!stringv_find_char(stringv_from(path, start), '/', &relative_end)) {
        relative_end = path.length - start;
    }
    *segment = stringv_slice(path, start, relative_end);
    *cursor = start + relative_end;
    return true;
}

static bool router_path_matches(const char *pattern_cstr, const char *path_cstr,
                                size_t *static_segment_count,
                                RouteParam *params, size_t *param_count) {
    StringView pattern = stringv_from_cstr(pattern_cstr);
    StringView path = stringv_from_cstr(path_cstr);
    size_t pattern_cursor = 0;
    size_t path_cursor = 0;
    size_t static_count = 0;
    size_t count = 0;
    StringView pattern_segment;
    StringView path_segment;

    if (!router_valid_path(pattern_cstr) || !router_valid_path(path_cstr)) {
        return false;
    }
    if (stringv_equal(pattern, sv("/")) || stringv_equal(path, sv("/"))) {
        bool matches_root =
            stringv_equal(pattern, sv("/")) && stringv_equal(path, sv("/"));
        if (matches_root && static_segment_count != NULL) {
            *static_segment_count = 0;
        }
        if (param_count != NULL) {
            *param_count = 0;
        }
        return matches_root;
    }

    while (router_next_segment(pattern, &pattern_cursor, &pattern_segment) &&
           router_next_segment(path, &path_cursor, &path_segment)) {
        if (pattern_segment.length > 0 && pattern_segment.data[0] == ':') {
            if (path_segment.length == 0 || pattern_segment.length == 1 ||
                count >= CEASY_MAX_ROUTE_PARAMS) {
                return false;
            }
            if (params != NULL) {
                params[count] =
                    (RouteParam){.name = stringv_from(pattern_segment, 1),
                                 .value = path_segment};
            }
            count++;
        } else {
            if (!stringv_equal(pattern_segment, path_segment)) {
                return false;
            }
            static_count++;
        }
    }
    if (pattern_cursor != pattern.length || path_cursor != path.length) {
        return false;
    }
    if (static_segment_count != NULL) {
        *static_segment_count = static_count;
    }
    if (param_count != NULL) {
        *param_count = count;
    }
    return true;
}

static const RouterRoute *router_find(const Router *router, HttpMethod method,
                                      const char *path, RouteParam *params,
                                      size_t *param_count) {
    const RouterRoute *best_route = NULL;
    size_t best_static_count = 0;
    RouteParam best_params[CEASY_MAX_ROUTE_PARAMS];
    size_t best_param_count = 0;

    for (size_t index = 0; index < router->route_count; index++) {
        const RouterRoute *route = &router->routes[index];
        RouteParam route_params[CEASY_MAX_ROUTE_PARAMS];
        size_t route_param_count = 0;
        size_t static_count;

        if (route->method != method ||
            !router_path_matches(route->path, path, &static_count, route_params,
                                 &route_param_count)) {
            continue;
        }
        if (best_route == NULL || static_count > best_static_count) {
            best_route = route;
            best_static_count = static_count;
            best_param_count = route_param_count;
            memcpy(best_params, route_params,
                   route_param_count * sizeof(best_params[0]));
        }
    }
    if (best_route != NULL && params != NULL) {
        memcpy(params, best_params, best_param_count * sizeof(params[0]));
    }
    if (param_count != NULL) {
        *param_count = best_route == NULL ? 0 : best_param_count;
    }
    return best_route;
}

void router_init(Router *router) {
    if (router != NULL) {
        memset(router, 0, sizeof(*router));
    }
}

void router_reset(Router *router) { router_init(router); }

Router *router_default(void) {
    static bool initialized;

    if (!initialized) {
        router_init(&default_router);
        initialized = true;
    }
    return &default_router;
}

bool router_add(Router *router, HttpMethod method, const char *path,
                RouteHandler handler) {
    if (router == NULL || method == HTTP_METHOD_UNKNOWN ||
        !router_valid_path(path) || handler == NULL ||
        router->route_count >= ROUTER_MAX_ROUTES) {
        return false;
    }
    router->routes[router->route_count++] =
        (RouterRoute){.path = path, .method = method, .handler = handler};
    return true;
}

bool router_route_get(Router *router, const char *path, RouteHandler handler) {
    return router_add(router, HTTP_METHOD_GET, path, handler);
}

bool router_route_post(Router *router, const char *path, RouteHandler handler) {
    return router_add(router, HTTP_METHOD_POST, path, handler);
}

bool router_route_patch(Router *router, const char *path,
                        RouteHandler handler) {
    return router_add(router, HTTP_METHOD_PATCH, path, handler);
}

bool router_route_delete(Router *router, const char *path,
                         RouteHandler handler) {
    return router_add(router, HTTP_METHOD_DELETE, path, handler);
}

bool router_default_route_get(const char *path, RouteHandler handler) {
    return router_route_get(router_default(), path, handler);
}

bool router_default_route_post(const char *path, RouteHandler handler) {
    return router_route_post(router_default(), path, handler);
}

bool router_default_route_patch(const char *path, RouteHandler handler) {
    return router_route_patch(router_default(), path, handler);
}

bool router_default_route_delete(const char *path, RouteHandler handler) {
    return router_route_delete(router_default(), path, handler);
}

bool router_path_exists(const Router *router, const char *path) {
    if (router == NULL || path == NULL) {
        return false;
    }
    for (size_t index = 0; index < router->route_count; index++) {
        if (router_path_matches(router->routes[index].path, path, NULL, NULL,
                                NULL)) {
            return true;
        }
    }
    return false;
}

RouterResult router_dispatch(Router *router, const char *method,
                             const char *path, Context *context) {
    RouteParam params[CEASY_MAX_ROUTE_PARAMS];
    size_t param_count = 0;
    const RouterRoute *route;

    if (router == NULL || method == NULL || path == NULL || context == NULL) {
        return ROUTER_NOT_FOUND;
    }
    context->route_param_count = 0;
    context->request.path = stringv_from_cstr(path);
    route = router_find(router, http_method_parse_cstr(method), path, params,
                        &param_count);
    if (route != NULL) {
        memcpy(context->route_params, params,
               param_count * sizeof(context->route_params[0]));
        context->route_param_count = param_count;
        route->handler(context);
        return ROUTER_DISPATCHED;
    }
    if (router_path_exists(router, path)) {
        return ROUTER_METHOD_NOT_ALLOWED;
    }
    return ROUTER_NOT_FOUND;
}

#include "ceasy/routing/router.h"

#include <string.h>

static Router default_router;

static bool router_valid_path(const char *path) {
    return path != NULL && path[0] == '/';
}

static bool router_next_segment(const char **cursor, const char **segment,
                                size_t *length) {
    const char *start = *cursor;
    const char *end;

    if (*start == '\0') {
        return false;
    }

    if (*start == '/') {
        start++;
    }

    end = strchr(start, '/');
    if (end == NULL) {
        end = start + strlen(start);
    }

    *segment = start;
    *length = (size_t)(end - start);
    *cursor = end;
    return true;
}

static bool router_path_matches(const char *pattern, const char *path,
                                size_t *static_segment_count) {
    const char *pattern_cursor = pattern;
    const char *path_cursor = path;
    const char *pattern_segment;
    const char *path_segment;
    size_t pattern_length;
    size_t path_length;
    size_t static_count = 0;

    if (!router_valid_path(pattern) || !router_valid_path(path)) {
        return false;
    }

    if (strcmp(pattern, "/") == 0 || strcmp(path, "/") == 0) {
        bool matches_root = strcmp(pattern, "/") == 0 && strcmp(path, "/") == 0;
        if (matches_root && static_segment_count != NULL) {
            *static_segment_count = 0;
        }
        return matches_root;
    }

    while (router_next_segment(&pattern_cursor, &pattern_segment,
                               &pattern_length) &&
           router_next_segment(&path_cursor, &path_segment, &path_length)) {
        if (pattern_length > 0 && pattern_segment[0] == ':') {
            if (path_length == 0) {
                return false;
            }
        } else {
            if (pattern_length != path_length ||
                memcmp(pattern_segment, path_segment, pattern_length) != 0) {
                return false;
            }
            static_count++;
        }
    }

    if (*pattern_cursor != '\0' || *path_cursor != '\0') {
        return false;
    }

    if (static_segment_count != NULL) {
        *static_segment_count = static_count;
    }
    return true;
}

static const RouterRoute *router_find(const Router *router, HttpMethod method,
                                      const char *path) {
    const RouterRoute *best_route = NULL;
    size_t best_static_count = 0;

    for (size_t index = 0; index < router->route_count; index++) {
        const RouterRoute *route = &router->routes[index];
        size_t static_count;

        if (route->method != method ||
            !router_path_matches(route->path, path, &static_count)) {
            continue;
        }

        if (best_route == NULL || static_count > best_static_count) {
            best_route = route;
            best_static_count = static_count;
        }
    }

    return best_route;
}

void router_init(Router *router) {
    if (router == NULL) {
        return;
    }

    memset(router, 0, sizeof(*router));
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

    router->routes[router->route_count].path = path;
    router->routes[router->route_count].method = method;
    router->routes[router->route_count].handler = handler;
    router->route_count++;
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
    return router_add(router, HTTP_METHOD_PUT, path, handler);
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

HttpMethod http_method_parse(const char *method) {
    if (method == NULL) {
        return HTTP_METHOD_UNKNOWN;
    }
    if (strcmp(method, "GET") == 0) {
        return HTTP_METHOD_GET;
    }
    if (strcmp(method, "POST") == 0) {
        return HTTP_METHOD_POST;
    }
    if (strcmp(method, "PATCH") == 0) {
        return HTTP_METHOD_PUT;
    }
    if (strcmp(method, "DELETE") == 0) {
        return HTTP_METHOD_DELETE;
    }
    return HTTP_METHOD_UNKNOWN;
}

bool router_path_exists(const Router *router, const char *path) {
    if (router == NULL || path == NULL) {
        return false;
    }

    for (size_t index = 0; index < router->route_count; index++) {
        if (router_path_matches(router->routes[index].path, path, NULL)) {
            return true;
        }
    }

    return false;
}

RouterResult router_dispatch(Router *router, const char *method,
                             const char *path, Context *context) {
    HttpMethod parsed_method;
    const RouterRoute *route;

    if (router == NULL || method == NULL || path == NULL || context == NULL) {
        return ROUTER_NOT_FOUND;
    }

    parsed_method = http_method_parse(method);
    route = router_find(router, parsed_method, path);
    if (route != NULL) {
        route->handler(context);
        return ROUTER_DISPATCHED;
    }

    if (router_path_exists(router, path)) {
        return ROUTER_METHOD_NOT_ALLOWED;
    }

    return ROUTER_NOT_FOUND;
}

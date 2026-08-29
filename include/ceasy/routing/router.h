#ifndef CEASY_ROUTER_H
#define CEASY_ROUTER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Context Context;

typedef void (*RouteHandler)(Context *context);

typedef enum {
    HTTP_METHOD_UNKNOWN = 0,
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE
} HttpMethod;

typedef struct {
    /* Path is borrowed; route paths should remain alive for router lifetime. */
    const char *path;
    HttpMethod method;
    RouteHandler handler;
} RouterRoute;

#define ROUTER_MAX_ROUTES 128

typedef struct {
    RouterRoute routes[ROUTER_MAX_ROUTES];
    size_t route_count;
} Router;

typedef enum {
    ROUTER_NOT_FOUND = 0,
    ROUTER_METHOD_NOT_ALLOWED,
    ROUTER_DISPATCHED
} RouterResult;

void router_init(Router *router);
void router_reset(Router *router);
Router *router_default(void);

bool router_add(Router *router, HttpMethod method, const char *path,
                RouteHandler handler);

bool router_route_get(Router *router, const char *path, RouteHandler handler);
bool router_route_post(Router *router, const char *path, RouteHandler handler);
bool router_route_patch(Router *router, const char *path, RouteHandler handler);
bool router_route_delete(Router *router, const char *path,
                         RouteHandler handler);

bool router_default_route_get(const char *path, RouteHandler handler);
bool router_default_route_post(const char *path, RouteHandler handler);
bool router_default_route_patch(const char *path, RouteHandler handler);
bool router_default_route_delete(const char *path, RouteHandler handler);

HttpMethod http_method_parse(const char *method);
bool router_path_exists(const Router *router, const char *path);
RouterResult router_dispatch(Router *router, const char *method,
                             const char *path, Context *context);

/* Supports route_get(router, path, handler) and route_get(path, handler). */
#define CEASY_ROUTE_SELECT(_1, _2, _3, NAME, ...) NAME
#define route_get(...)                                                         \
    CEASY_ROUTE_SELECT(__VA_ARGS__, router_route_get,                          \
                       router_default_route_get, unused)                       \
    (__VA_ARGS__)
#define route_post(...)                                                        \
    CEASY_ROUTE_SELECT(__VA_ARGS__, router_route_post,                         \
                       router_default_route_post, unused)                      \
    (__VA_ARGS__)
#define route_patch(...)                                                       \
    CEASY_ROUTE_SELECT(__VA_ARGS__, router_route_patch,                        \
                       router_default_route_patch, unused)                     \
    (__VA_ARGS__)
#define route_delete(...)                                                      \
    CEASY_ROUTE_SELECT(__VA_ARGS__, router_route_delete,                       \
                       router_default_route_delete, unused)                    \
    (__VA_ARGS__)

#endif

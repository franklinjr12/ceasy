#ifndef CEASY_CONTEXT_H
#define CEASY_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

#include <ceasy/database/database.h>
#include <ceasy/http/cookie.h>
#include <ceasy/http/request.h>
#include <ceasy/memory/arena.h>
#include <ceasy/session/session.h>
#include <ceasy/string/string.h>
#include <ceasy/view/view.h>

#define CEASY_MAX_FORM_PARAMS 64
#define CEASY_MAX_ROUTE_PARAMS 16
#define CEASY_MAX_QUERY_PARAMS 64
#define CEASY_MAX_RESPONSE_HEADERS 32
#define CEASY_MAX_REQUEST_COOKIES 64

typedef struct {
    StringView name;
    StringView value;
} ResponseHeader;

typedef struct {
    StringView name;
    StringView value;
} QueryParam;

typedef struct {
    StringView name;
    StringView value;
} FormParam;

typedef struct {
    StringView name;
    StringView value;
} RouteParam;

typedef struct Context {
    int client_fd;
    Request request;
    Database *database;
    Arena *arena;
    bool response_sent;
    ViewData view_data;
    StringView view_error;

    FormParam form_params[CEASY_MAX_FORM_PARAMS];
    size_t form_count;
    bool forms_parsed;
    bool form_parse_ok;

    QueryParam query_params[CEASY_MAX_QUERY_PARAMS];
    size_t query_count;
    bool query_parsed;
    bool query_parse_ok;

    CookieParam cookies[CEASY_MAX_REQUEST_COOKIES];
    size_t cookie_count;
    bool cookies_parsed;
    bool cookie_parse_ok;

    ResponseHeader response_headers[CEASY_MAX_RESPONSE_HEADERS];
    size_t response_header_count;

    Session session;
    StringView request_id;

    RouteParam route_params[CEASY_MAX_ROUTE_PARAMS];
    size_t route_param_count;
} Context;

/* Missing and present-empty fields both return an empty view. */
StringView context_form(Context *context, StringView name);
bool context_parse_form(Context *context);
bool context_parse_query(Context *context);
StringView context_query(Context *context, StringView name);
/* Route parameter views borrow request storage. */
StringView context_param(Context *context, StringView name);
StringView context_cookie(Context *context, StringView name);
bool context_set_header(Context *context, StringView name, StringView value);
bool context_add_header(Context *context, StringView name, StringView value);

#endif

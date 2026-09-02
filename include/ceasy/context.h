#ifndef CEASY_CONTEXT_H
#define CEASY_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

#include <ceasy/database/database.h>
#include <ceasy/http/request.h>
#include <ceasy/memory/arena.h>
#include <ceasy/string/string.h>

#define CEASY_MAX_FORM_PARAMS 64
#define CEASY_MAX_ROUTE_PARAMS 16

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

    FormParam form_params[CEASY_MAX_FORM_PARAMS];
    size_t form_count;
    bool forms_parsed;
    bool form_parse_ok;

    RouteParam route_params[CEASY_MAX_ROUTE_PARAMS];
    size_t route_param_count;
} Context;

/* Missing and present-empty fields both return an empty view. */
StringView context_form(Context *context, StringView name);
bool context_parse_form(Context *context);
/* Route parameter views borrow request storage. */
StringView context_param(Context *context, StringView name);

#endif

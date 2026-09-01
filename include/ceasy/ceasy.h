#ifndef CEASY_H
#define CEASY_H

#include <ceasy/database/database.h>
#include <ceasy/http/http_server.h>
#include <ceasy/memory/arena.h>
#include <ceasy/routing/router.h>

#include <stdbool.h>

#define CEASY_REQUEST_SIZE 4096

typedef struct Context {
    char request[CEASY_REQUEST_SIZE];
    int client_fd;
    Database *database;
    bool response_sent;
} Context;

/* Application route hook. Define this in application code to replace defaults.
 */
void routes(Router *router);

bool context_send_response(Context *context, const char *status,
                           const char *content_type, const char *body);
bool context_send_text(Context *context, const char *status, const char *body);

void ceasy_run(int argc, char **argv);

#endif

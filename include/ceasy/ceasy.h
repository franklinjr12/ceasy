#ifndef CEASY_H
#define CEASY_H

#include <ceasy/context.h>
#include <ceasy/database/database.h>
#include <ceasy/http/http_server.h>
#include <ceasy/http/request.h>
#include <ceasy/memory/arena.h>
#include <ceasy/model/model.h>
#include <ceasy/rendering/html.h>
#include <ceasy/routing/router.h>
#include <ceasy/string/string.h>
#include <ceasy/view/view.h>

/* Application route hook. Define this in application code to replace defaults.
 */
void routes(Router *router);

bool context_send_response(Context *context, StringView status,
                           StringView content_type, StringView body);
bool context_send_text(Context *context, StringView status, StringView body);
bool context_send_html(Context *context, StringView status, StringView body);
bool context_redirect(Context *context, StringView location);

void ceasy_run(int argc, char **argv);

#endif

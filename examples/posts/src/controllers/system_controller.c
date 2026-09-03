#include "system_controller.h"

void health_index(Context *context) {
    context_send_text(context, sv("200 OK"), sv("ok\n"));
}

void ready_index(Context *context) {
    DatabaseStatement statement = {0};
    bool ok = database_prepare(context->database, &statement, sv("SELECT 1")) &&
              database_step(&statement) == DATABASE_STEP_ROW;
    database_statement_destroy(&statement);
    context_send_text(context,
                      ok ? sv("200 OK") : sv("503 Service Unavailable"),
                      ok ? sv("ready\n") : sv("not ready\n"));
}
